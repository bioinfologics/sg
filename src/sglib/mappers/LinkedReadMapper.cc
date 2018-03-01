//
// Created by Bernardo Clavijo (EI) on 03/11/2017.
//
#include <iostream>
#include <iomanip>
#include <cassert>
#include <atomic>
#include <omp.h>
#include "LinkedReadMapper.hpp"

void LinkedReadMapper::write(std::ofstream &output_file) {
    //read-to-node
    uint64_t count=read_to_node.size();
    output_file.write((const char *) &count,sizeof(count));
    output_file.write((const char *) read_to_node.data(),sizeof(sgNodeID_t)*count);
    //mappings
    count=reads_in_node.size();
    output_file.write((const char *) &count,sizeof(count));
    for (auto i=0;i<count;++i) {
        uint64_t mcount=reads_in_node[i].size();
        output_file.write((const char *) &mcount,sizeof(mcount));
        output_file.write((const char *) reads_in_node[i].data(), sizeof(ReadMapping) * mcount);
    }
}

void LinkedReadMapper::read(std::ifstream &input_file) {
    uint64_t count;
    input_file.read(( char *) &count,sizeof(count));
    read_to_node.resize(count);
    input_file.read(( char *) read_to_node.data(),sizeof(sgNodeID_t)*count);
    input_file.read(( char *) &count,sizeof(count));
    reads_in_node.resize(count);
    for (auto i=0;i<count;++i) {
        uint64_t mcount;
        input_file.read(( char *) &mcount,sizeof(mcount));
        reads_in_node[i].resize(mcount);
        input_file.read(( char *) reads_in_node[i].data(), sizeof(ReadMapping) * mcount);
    }
}

void LinkedReadMapper::update_graph_index() {
    const int k = 31;
    const int max_coverage = 1;
    const std::string output_prefix("./");

    SMR<KmerIDX,
            kmerIDXFactory<FastaRecord>,
            GraphNodeReader<FastaRecord>,
            FastaRecord,
            GraphNodeReaderParams,
            KMerIDXFactoryParams> kmerIDX_SMR({1, sg}, {k}, memlimit, 0, max_coverage, output_prefix);

   // Get the unique_kmers from the graph into a map
    std::cout << "Indexing graph... " << std::endl;
    kmer_to_graphposition.clear();
    reads_in_node.resize(sg.nodes.size());
    std::unordered_set<int32_t> seen_contigs;
    for (auto &kidx :kmerIDX_SMR.process_from_memory()) {
        kmer_to_graphposition[kidx.kmer]={kidx.contigID,kidx.pos};
        seen_contigs.insert((kidx.contigID>0?kidx.contigID:-kidx.contigID));
    }
    std::cout<<seen_contigs.size()<<" with indexed kmers"<<std::endl;

    std::vector<uint64_t> uniqKmer_statistics(kmerIDX_SMR.summaryStatistics());
    std::cout << "Number of sequences in graph: " << uniqKmer_statistics[2] << std::endl;
    std::cout << "Number of " << int(k) << "-kmers in graph " << uniqKmer_statistics[0] << std::endl;
    std::cout << "Number of " << int(k) << "-kmers in graph index " << uniqKmer_statistics[1] << std::endl;
}

class StreamKmerFactory : public  KMerFactory {
public:
    explicit StreamKmerFactory(uint8_t k) : KMerFactory(k){}
    inline void produce_all_kmers(const int64_t seqID, const char * seq, std::vector<KmerIDX> &mers){
        // TODO: Adjust for when K is larger than what fits in uint64_t!
        last_unknown=0;
        fkmer=0;
        rkmer=0;
        auto s=seq;
        while (*s!='\0' and *s!='\n') {
            //fkmer: grows from the right (LSB)
            //rkmer: grows from the left (MSB)
            fillKBuf(*s, 0, fkmer, rkmer, last_unknown);
            if (last_unknown >= K) {
                if (fkmer <= rkmer) {
                    // Is fwd
                    mers.emplace_back(fkmer);
                } else {
                    // Is bwd
                    mers.emplace_back(rkmer);
                }
            }
            ++s;
        }
    }
};


void LinkedReadMapper::map_reads(const std::unordered_set<uint64_t> &reads_to_remap) {
    const int k = 31;
    std::cout<<"mapping reads!!!"<<std::endl;
    read_to_node.resize(datastore.size()+1);
    std::cout<<reads_to_remap.size()<<" selected reads / "<<read_to_node.size()-1<<" total"<<std::endl;

    /*
     * Read mapping in parallel,
     */
    uint64_t thread_mapped_count[omp_get_max_threads()],thread_total_count[omp_get_max_threads()],thread_multimap_count[omp_get_max_threads()];
    std::vector<ReadMapping> thread_mapping_results[omp_get_max_threads()];
    sglib::OutputLog(sglib::LogLevels::DEBUG)<<"Private mapping initialised for "<<omp_get_max_threads()<<" threads"<<std::endl;
#pragma omp parallel
    {
        const int min_matches=1;
        std::vector<KmerIDX> readkmers;
        StreamKmerFactory skf(31);
        ReadMapping mapping;
        auto blrs=BufferedLRSequenceGetter(datastore,128*1024,260);
        auto & private_results=thread_mapping_results[omp_get_thread_num()];
        auto & mapped_count=thread_mapped_count[omp_get_thread_num()];
        auto & total_count=thread_total_count[omp_get_thread_num()];
        auto & multimap_count=thread_multimap_count[omp_get_thread_num()];
        mapped_count=0;
        total_count=0;
        multimap_count=0;
        bool c ;
        //std::cout<<omp_get_thread_num()<<std::endl;
#pragma omp for
        for (uint64_t readID=1;readID<read_to_node.size();++readID) {
            mapping.read_id = readID;
            //this enables partial read re-mapping by setting read_to_node to 0
            if ((reads_to_remap.size()>0 and reads_to_remap.count(mapping.read_id)>0) or (reads_to_remap.empty() and 0==read_to_node[mapping.read_id])) {
                mapping.node = 0;
                mapping.unique_matches = 0;
                mapping.first_pos = 0;
                mapping.last_pos = 0;
                mapping.rev = false;
                mapping.unique_matches = 0;
                //get all kmers from read
                auto seq=blrs.get_read_sequence(readID);
                readkmers.clear();
                skf.produce_all_kmers(readID,seq,readkmers);

                for (auto &rk:readkmers) {
                    auto nk = kmer_to_graphposition.find(rk.kmer);
                    if (kmer_to_graphposition.end()!=nk) {
                        //get the node just as node
                        sgNodeID_t nknode = (nk->second.node > 0 ? nk->second.node : -nk->second.node);
                        //TODO: sort out the sign/orientation representation
                        if (mapping.node == 0) {
                            mapping.node = nknode;
                            if ((nk->second.node > 0 and rk.contigID > 0) or
                                (nk->second.node < 0 and rk.contigID < 0))
                                mapping.rev = false;
                            else mapping.rev = true;
                            mapping.first_pos = nk->second.pos;
                            mapping.last_pos = nk->second.pos;
                            ++mapping.unique_matches;
                        } else {
                            if (mapping.node != nknode) {
                                mapping.node = 0;
                                ++multimap_count;
                                break; //exit -> multi-mapping read! TODO: allow mapping to consecutive nodes
                            } else {
                                mapping.last_pos = nk->second.pos;
                                ++mapping.unique_matches;
                            }
                        }
                    }
                }
                if (mapping.node != 0 and mapping.unique_matches >= min_matches) {
                    //optimisation: just save the mapping in a thread private collection for now, have a single thread putting from that into de structure at the end
                    private_results.push_back(mapping);
                    ++mapped_count;
                }
            }
            auto tc = ++total_count;
            if (tc % 10000000 == 0) std::cout << mapped_count << " / " << tc <<" ("<<multimap_count<<" multi-mapped)"<< std::endl;
        }
    }
    for (auto & tres:thread_mapping_results){
        sglib::OutputLog(sglib::LogLevels::DEBUG)<<"mixing in "<<tres.size()<<" thread specific results"<<std::endl;
        for (auto &rm:tres){
            read_to_node[rm.read_id] = rm.node;
            reads_in_node[rm.node].emplace_back(rm);
        }
        tres.clear();
        tres.shrink_to_fit();
    }
    uint64_t mapped_count=0,total_count=0,multimap_count=0;
    for (auto i=0;i<omp_get_max_threads();++i){
        mapped_count+=thread_mapped_count[i];
        total_count+=thread_total_count[i];
        multimap_count+=thread_multimap_count[i];
    }
    sglib::OutputLog(sglib::LogLevels::INFO)<<"Reads mapped: "<<mapped_count<<" / "<<total_count<<" ("<<multimap_count<<" multi-mapped)"<<std::endl;
#pragma omp parallel for
    for (sgNodeID_t n=1;n<reads_in_node.size();++n){
        std::sort(reads_in_node[n].begin(),reads_in_node[n].end());
    }


}

///**
// * @brief Mapping of paired end read files.
// *
// * Reads are mapped through a unique k-mer index in process_reads_from_file.
// * R1 and R2 are processed independently. R1 gets odds ids, R2 gets the next even id, so [1,2] and [3,4] are pairs.
// *
// * @todo fix and test 10x tags processing
// * @todo enable some basic 10x tag statistics
// * @todo add support for LMP reads (i.e FR reads)
// * @todo add distribution computing on the fly
// * @todo support other kind of indexes and variable k-mer size
// */
//void LinkedReadMapper::map_reads(std::string filename1, std::string filename2, prmReadType read_type, uint64_t max_mem) {
//    read1filename=std::move(filename1);
//    read2filename=std::move(filename2);
//    readType=read_type;
//    memlimit=max_mem;
//    remap_reads();
//}

void LinkedReadMapper::remove_obsolete_mappings(){
    uint64_t nodes=0,reads=0;
    std::set<sgNodeID_t> updated_nodes;
    for (auto n=1;n<sg.nodes.size();++n) {
        if (sg.nodes[n].status==sgNodeDeleted) {
            updated_nodes.insert(n);
            updated_nodes.insert(-n);
            reads_in_node[n > 0 ? n : -n].clear();
            ++nodes;
        }
    }
    for (auto &read_node:read_to_node) {
        if (updated_nodes.count(read_node) !=0 ) {
            read_node=0;
            ++reads;
        }
    }
    std::cout << "obsolete mappings removed from "<<nodes<<" nodes, total "<<reads<<" reads."<<std::endl;
}

std::unordered_set<bsg10xTag> LinkedReadMapper::get_node_tags(sgNodeID_t n) {
    std::unordered_set<bsg10xTag> tags;
    for (auto &rm:reads_in_node[(n>0?n:-n)])
        tags.insert(datastore.get_read_tag(rm.read_id));
    if (tags.count(0)>0) tags.erase(0);
    return tags;
}