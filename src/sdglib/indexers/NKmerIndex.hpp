//
// Created by Luis Yanes (EI) on 17/08/2018.
//

#ifndef BSG_NKMERINDEX_HPP
#define BSG_NKMERINDEX_HPP

#include <vector>
#include <sdglib/utilities/omp_safe.hpp>
#include <sdglib/types/KmerTypes.hpp>
#include <sdglib/factories/KMerFactory.hpp>
#include <sdglib/logger/OutputLog.hpp>
#include <sdglib/graph/SequenceDistanceGraph.hpp>
#include <sdglib/bloom/BloomFilter.hpp>

class NKmerIndex {
    BloomFilter filter;
    std::vector<kmerPos> assembly_kmers;
    uint8_t k=15;
public:
    using const_iterator = std::vector<kmerPos>::const_iterator;

    NKmerIndex(){}
    explicit NKmerIndex(uint8_t k) : k(k), filter(70*1024*1024) {}

    uint64_t filter_kmers(std::vector<kmerPos> &kmers, int max_kmer_repeat) {
        uint64_t total_kmers(0);
        auto witr = kmers.begin();
        auto ritr = witr;
        for (; ritr != kmers.end();) {
            auto bitr = ritr;
            while (ritr != kmers.end() and bitr->kmer == ritr->kmer) {
                ++ritr;
            }
            if (ritr - bitr < max_kmer_repeat) {
                total_kmers+=1;
                while (bitr != ritr) {
                    *witr = *bitr;
                    ++witr;
                    ++bitr;
                }
            }
        }
        kmers.resize(witr-kmers.begin());
        return total_kmers;
    }

    void generate_index(const SequenceDistanceGraph &sg, int filter_limit = 200, bool verbose=true) {
        uint64_t total_length=0;
#pragma omp parallel for reduction(+:total_length)
        for (sgNodeID_t n = 1; n < sg.nodes.size(); ++n) {
            total_length+=sg.nodes[n].sequence.size();
        }
        assembly_kmers.reserve(total_length);
        if (verbose) sdglib::OutputLog() << "Updating mapping index for k=" << std::to_string(k) << std::endl;
#pragma omp parallel
        {
            StringKMerFactory skf(k);
            std::vector<kmerPos> local_kmers;
            local_kmers.reserve(total_length/(omp_get_num_threads()*4));
            std::vector<std::pair<bool,uint64_t > > contig_kmers;
            contig_kmers.reserve(10000000);
#pragma omp for
            for (sgNodeID_t n = 1; n < sg.nodes.size(); ++n) {
                if (sg.nodes[n].sequence.size() >= k) {
                    contig_kmers.clear();
                    skf.create_kmers(sg.nodes[n].sequence, contig_kmers);
                    int k_i(0);
                    for (const auto &kmer:contig_kmers) {
                        local_kmers.emplace_back(kmer.second, n, kmer.first ? k_i + 1 : -(k_i + 1));
                        k_i++;
                    }
                }

                if (local_kmers.size() > 800000000) {
#pragma omp critical(push_kmers)
                    {
                        assembly_kmers.insert(assembly_kmers.end(), local_kmers.begin(), local_kmers.end());
                    }
                    local_kmers.clear();
                }
            }
            if (!local_kmers.empty()) {
#pragma omp critical(push_kmers)
                {
                    assembly_kmers.insert(assembly_kmers.end(), local_kmers.begin(), local_kmers.end());
                }
                local_kmers.clear();
            }
        }


        sdglib::sort(assembly_kmers.begin(),assembly_kmers.end(), kmerPos::byKmerContigOffset());

        if (verbose) sdglib::OutputLog() << "Filtering kmers appearing less than " << filter_limit << " from " << assembly_kmers.size() << " initial kmers" << std::endl;
        auto total_kmers(filter_kmers(assembly_kmers, filter_limit));
#pragma omp parallel for
        for (uint64_t kidx = 0; kidx < assembly_kmers.size(); ++kidx) {
            filter.add(assembly_kmers[kidx].kmer);
        }

        if (verbose) sdglib::OutputLog() << "Elements for mapping " << assembly_kmers.size() << std::endl;
        if (verbose) sdglib::OutputLog() << "Total distinct kmers " << total_kmers << std::endl;
        if (verbose) sdglib::OutputLog() << "Number of elements in bloom " << filter.number_bits_set() << std::endl;
        if (verbose) sdglib::OutputLog() << "Filter FPR " << filter.false_positive_rate() << std::endl;
        if (verbose) sdglib::OutputLog() << "DONE" << std::endl;
    }

    bool empty() const { return assembly_kmers.empty(); }
    const_iterator begin() const {return assembly_kmers.cbegin();}
    const_iterator end() const {return assembly_kmers.cend();}

    const_iterator find(const uint64_t kmer) const {
        if (filter.contains(kmer)) { return std::lower_bound(assembly_kmers.cbegin(), assembly_kmers.cend(), kmer); }
        return assembly_kmers.cend();
//        return std::lower_bound(assembly_kmers.cbegin(), assembly_kmers.cend(), kmer);
    }
};


#endif //BSG_NKMERINDEX_HPP