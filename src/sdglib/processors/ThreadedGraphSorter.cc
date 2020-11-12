//
// Created by Gonzalo Garcia (EI) on 2020-11-10.
//

#include "ThreadedGraphSorter.h"

std::map<sgNodeID_t , int64_t > sort_cc(const DistanceGraph& dg, std::unordered_set<sgNodeID_t> cc){
    //uses relative position propagation to create a total order for a connected component
    //    returns a dict of nodes to starting positions, and a sorted list of nodes with their positions
    // Next nodes to process
    std::vector<sgNodeID_t > next_nodes;
    // Map with the nodes positions
    std::map<sgNodeID_t , int64_t > node_pos;

    // check no node on the CC appears in both directions.
    for (const auto&x: cc){
        if (cc.find(-x) != cc.end()){
            return {};
        }
    }

    // now start from each node without prev as 0
    // Nodes with no prev are the inital nodes in the partial order
    for (const auto& x: cc){
        if (dg.get_nodeview(x).prev().empty()){
            next_nodes.push_back(x);
        }
        node_pos[x]=0;
    }

    // Propagate position FW
    auto pass=0;
    while (!next_nodes.empty()) {
        std::vector<sgNodeID_t > new_next_nodes;
        for (const auto& nid: next_nodes){
            for (const auto& l: dg.get_nodeview(nid).next()){
                // TODO: does this work like this --> if nid node ends after the linked node the next node is moved fw and the linked node is added to the list to place
                if (node_pos[nid]+dg.get_nodeview(nid).size()+l.distance()>node_pos[l.node().node_id()]){
                    node_pos[l.node().node_id()] = node_pos[nid]+dg.get_nodeview(nid).size()+l.distance();
                    new_next_nodes.push_back(l.node().node_id());
                }
            }
        }
        next_nodes=new_next_nodes;
        if (++pass>cc.size()){
            return {};
        };
    }
    // move all nodes with no next to the end by asigning the max position to all nodes with no next
    int64_t lastp=0;
    for (const auto& mi: node_pos){
        if (mi.second > lastp) lastp=mi.second;
    }
    for (const auto& nid: cc){
        if (dg.get_nodeview(nid).next().empty()){
            node_pos[nid]=lastp;
        }
    }

    // move every node up to its limit fw
    bool updated=true;
    pass=0;
    while (updated){
        updated=false;
        //sort node_pos by the second element
        // TODO: This sorted_node_pos could be copies once to the vector outside the loop and then work with the vector until return
        std::vector<std::pair<sgNodeID_t , int64_t>> sorted_node_pos;
        std::copy(node_pos.begin(), node_pos.end(), std::back_inserter< std::vector<std::pair<sgNodeID_t , int64_t>>>(sorted_node_pos));
        std::sort(sorted_node_pos.begin(), sorted_node_pos.end(), [](std::pair<sgNodeID_t , int64_t> elem1 ,std::pair<sgNodeID_t , int64_t> elem2){return elem1.second < elem2.second;});

        for (const auto& mp: sorted_node_pos){
            auto nid = mp.first;
            auto p = node_pos[mp.first];
            auto nid_node_size = dg.get_nodeview(nid).size();

            if (!dg.get_nodeview(nid).next().empty()){
                // get min distance between the nid and the others, means the next node
                int64_t np = std::numeric_limits<int64_t >::max;
                for (const auto& l: dg.get_nodeview(nid).next()){
                    auto linked_node_id = l.node().node_id();
                    if (node_pos[linked_node_id]-l.distance()-nid_node_size < np)
                        np = node_pos[linked_node_id]-l.distance()-nid_node_size;
                }
                if (np!=node_pos[nid]){
                    node_pos[nid]=np;
                    updated=true;
                }

            }
        }
        if (++pass>cc.size()){
            return {};
        };
    }
    // move every node up to its limit bw
    updated=true;
    pass=0;
    while(updated){
        updated=false;
        // TODO: This sorted_node_pos could be copies once to the vector outside the loop and then work with the vector until return
        std::vector<std::pair<sgNodeID_t , int64_t>> sorted_node_pos;
        std::copy(node_pos.begin(), node_pos.end(), std::back_inserter< std::vector<std::pair<sgNodeID_t , int64_t>>>(sorted_node_pos));
        std::sort(sorted_node_pos.begin(), sorted_node_pos.end(), [](std::pair<sgNodeID_t , int64_t> elem1 ,std::pair<sgNodeID_t , int64_t> elem2){return elem1.second < elem2.second;});

        for (const auto& mp: sorted_node_pos){
            auto nid = mp.first;
            auto p = node_pos[mp.first];
            auto nid_node_size = dg.get_nodeview(nid).size();

            if (!dg.get_nodeview(nid).prev().empty()){
                // get min distance between the nid and the others, means the next node
                int64_t np = 0;
                for (const auto& l: dg.get_nodeview(nid).prev()){
                    auto linked_node_id = l.node().node_id();
                    if (node_pos[linked_node_id]+l.distance()+l.node().size() > np)
                        np = node_pos[linked_node_id]+l.distance()+l.node().size();
                }
                if (np!=node_pos[nid]){
                    node_pos[nid]=np;
                    updated=true;
                }
            }
        }
        if (++pass>cc.size()){
            return {};
        };
    }
    return node_pos;
}

void TheGreedySorter::initialize() {

    // fill all nodes vector
    for (const auto& n: trg_nt.get_all_nodeviews(false, false)){
        all_nodes.push_back(n.node_id());
    }

    // filling the all reads collection
    std::vector<uint64_t > loopy_reads; // <- TODO:
    std::unordered_set<uint64_t > discarded_reads;
    for (const auto& nv: trg_nt.get_all_nodeviews(false, false)){
//        std::unordered_map<uint64_t, int> c;
        for (const auto& lv: nv.next()){
            all_reads.insert(lv.support().id);
//            c[lv.support().id]++;
        }
        for (const auto& lv: nv.prev()){
            all_reads.insert(lv.support().id);
//            c[lv.support().id]++;
        }

//        for (const auto& rc: c){
//            if (rc.second>2){
//                std::cout << "node" << nv.node_id() << " has " << rc.second << " links from read " << rc.first << std::endl;
//                discarded_reads.insert(rc.first);
//            }
//        }
    }

//    std::cout << "Discarding " << discarded_reads.size() << " reads that have >2 links on at least one node" << std::endl;
    // dete reads from all_reads
//    for (const auto& dr: discarded_reads)
//        all_reads.erase(dr);


//    // remove all links from discarded reads
//    int dlinks=0;
//    for (const auto& nv: trg_nt.get_all_nodeviews(false, false)){
//        for (const auto& l: nv.next()){
//            if (discarded_reads.find(l.support().id) != discarded_reads.end()){
//                dlinks++;
//                trg_nt.remove_link(-nv.node_id(), l.node().node_id(), l.distance(), l.support());
//            }
//        }
//    }
//    std::cout << dlinks << " links discarded" << std::endl;

    std::cout << "finding read ends" << std::endl;
//    std::vector<sgNodeID_t > used_nodes;
//    std::vector<sgNodeID_t > used_reads;
//    std::map<uint64_t, std::vector<sgNodeID_t >> read_ends;
    for (const auto& nv: trg_nt.get_all_nodeviews(false, false)){
        // fill fw and bw rids support vectors
        std::unordered_set<sgNodeID_t > frids;
        for (const auto& x: nv.next())
            frids.insert(x.support().id);
        std::unordered_set<sgNodeID_t > brids;
        for (const auto& x: nv.prev())
            brids.insert(x.support().id);

        // Checks if this rid is at the end of the available support
        for (const auto& rid: frids){
            if (brids.find(rid)==brids.end()){
                if (read_ends.find(rid)==read_ends.end()){
                    read_ends[rid]={};
                }
                read_ends[rid].push_back(nv.node_id());
            }
        }
        // Checks if this rid is at the end of the available support
        for (const auto& rid: brids){
            if (frids.find(rid) == frids.end()) {
                if (read_ends.find(rid)==read_ends.end()) {
                    read_ends[rid]={};
                }
                read_ends[rid].push_back(-nv.node_id());
            }
        }
    }
    std::cout<<"finding loopy reads"<< std::endl;
    for (const auto& rid: all_reads){
        auto nv = trg_nt.get_nodeview(read_ends[rid][0]);
        std::unordered_set<sgNodeID_t > seen = {llabs(nv.node_id())};
        while (true) {
            std::vector<LinkView> lf;
            for (const auto& x: nv.next()){
                if (x.support().id == rid){
                    lf.push_back(x);
                }
            }
            if (lf.empty()) break;
            nv = lf[0].node();
            if (seen.find(llabs(nv.node_id()))!=seen.end()){
                discarded_reads.insert(rid);
                break;
            } else {
                seen.insert(llabs(nv.node_id()));
            }
        }
    }
    std::cout << "discarding " << discarded_reads.size() << " loopy reads" << std::endl;
    for (const auto& dr: discarded_reads)
        all_reads.erase(dr);

    int dlinks=0;
    for (const auto& nv: trg_nt.get_all_nodeviews(true, false)){
        for (const auto& l: nv.next()){
            if (discarded_reads.find(l.support().id) != discarded_reads.end()) {
                dlinks+=1;
                trg_nt.remove_link(-nv.node_id(), l.node().node_id(), l.distance(), l.support());
            }
        }
    }
    std::cout << "discarded " << dlinks << " links" << std::endl;
    std::cout << "TheGreedySorter created with" << all_nodes.size() << "nodes and reads "<< all_reads.size() << std::endl;
}

std::vector<uint64_t > TheGreedySorter::rids_from_node(NodeView nv){
    // TODO: report that the rids are uint64_t but the ids in support are int64_t
    std::unordered_set<uint64_t > rids;
    for (const auto& lv: nv.next()){
        rids.insert((uint64_t)lv.support().id);
    }
    for (const auto& lv: nv.prev()){
        rids.insert((uint64_t)lv.support().id);
    }
    return std::vector<uint64_t >(rids.begin(), rids.end());
}

uint64_t TheGreedySorter::shared_reads(NodeView nv1, NodeView nv2){
    uint64_t shared=0;
    auto r1 = rids_from_node(nv1);
    auto r2 = rids_from_node(nv2);
    std::sort(r1.begin(), r1.end());
    std::sort(r2.begin(), r2.end());

    int pr1=0;
    int pr2=0;
    while (pr1<r1.size() and pr2<r2.size()){
        if (r1[pr1]<r2[pr2]){
            pr1++;
        } else if (r1[pr1]>r2[pr2]){
            pr2++;
        } else {
            shared++;
            pr1++;
            pr2++;
        }
    }
    return shared;
}

bool TheGreedySorter::pop_node(sgNodeID_t node_id, uint64_t read){
    auto nv = dg.get_nodeview(node_id);
    std::vector<LinkView> ln;
    std::vector<LinkView> lp;
    // Get reads supporting node connections
    for (const auto& lv: nv.next()){
        if (lv.support().id == read) ln.push_back(lv);
    }
    for (const auto& lv: nv.prev()){
        if (lv.support().id == read) lp.push_back(lv);
    }

    // Tip case
    if (ln.empty() and lp.size()==1){
        dg.remove_link(-lp[0].node().node_id(), node_id, lp[0].distance(), lp[0].support());
        return true;
    } else if (ln.size() == 1 and lp.empty()) {
        dg.remove_link(-node_id, ln[0].node().node_id(), ln[0].distance(), ln[0].support());
        return true;
    }

    // TODO ask here!! --> node is tip or it's connected to itself???
    if (ln.size()!=1 or lp.size()!=1 or llabs(ln[0].node().node_id()) == llabs(node_id) or llabs(lp[0].node().node_id()) == llabs(node_id))
        return false;
    // Pops the node from the line
    uint32_t td = ln[0].distance()+nv.size()+lp[0].distance();
    dg.add_link(-lp[0].node().node_id(), ln[0].node().node_id(), td, lp[0].support());
    dg.remove_link(-node_id,ln[0].node().node_id(),ln[0].distance(),ln[0].support());
    dg.remove_link(-lp[0].node().node_id(),node_id,lp[0].distance(),lp[0].support());
    return true;
}

void TheGreedySorter::start_from_read(uint64_t rid, int min_confirmation){
    // only pick the safest nodes alongside a read (i.e. they have a minimum number of confirming other reads shared)
    auto nv = trg_nt.get_nodeview(read_ends[rid][0]);
    int links_added=0;
    while (true){

        std::vector<LinkView> vlf;
        for (const auto& x: nv.next()){
            if (x.support().id == rid){
                vlf.push_back(x);
            }
        }
        if (vlf.empty()) break;
        auto lf = vlf[0]; // TODO: question: this lf[0] what if there are more lf, why we don't expect to find more anywhere in the code?

//        std::cout << "Adding link" << -nv.node_id() << "-->" << lf.node().node_id() << "(" << lf.distance() << " " << lf.support().id << ")" << std::endl;
        dg.add_link(-nv.node_id(), lf.node().node_id(), lf.distance(), lf.support());
        nv = lf.node();
        links_added++;
    }

    std::cout << links_added<< " links added" << std::endl;
    // Pop all nodes with no minim support confirmation bw or fw
    bool popped=true;
    int nodes_popped=0;
    while (popped) {
        popped=false;
        int nvs_analysed = 0;
        for (const auto& nn: dg.get_all_nodeviews(true, false)){
            nvs_analysed++;
            if (!nn.next().empty()){
                // shared reads checks support of the current nv id in the multigra that stores all links from the reads (that why the nv roundabout, nv from different graphs)
                if (shared_reads(trg_nt.get_nodeview(nn.node_id()), trg_nt.get_nodeview(nn.next()[0].node().node_id())) <= min_confirmation ) {
                    pop_node(nn.node_id(), rid);
                    popped=true;
                    nodes_popped++;
                }
            }
        }
        if (nvs_analysed<10){
            std::cout<< "Aborting since there are less than 10 nodes left" << std::endl;
            break;
        }
    }
    std::cout << nodes_popped << " nodes popped" << std::endl;
    for (const auto& nn: dg.get_all_nodeviews(false, false)){
        used_nodes.push_back(nn.node_id());
    }
    used_reads.push_back(rid);
}

std::map<sgNodeID_t , int64_t > TheGreedySorter::sort_graph(){
    return sort_cc(dg, dg.get_connected_component(used_nodes[0]));
}

std::pair<int, int> TheGreedySorter::evaluate_read(uint64_t rid, bool print_pos){
    // Walk through a read in the original graph, count how many nodes are in the sorted graph, then check their order
    std::map<sgNodeID_t , int64_t > current_node_pos;
    if (print_pos){
        current_node_pos = sort_graph();
    }

    auto nv = trg_nt.get_nodeview(read_ends[rid][0]);
    int used = 0;
    int unused = 0;
    std::unordered_set<sgNodeID_t > seen={llabs(nv.node_id())};
    while (true) {
        auto nnv = dg.get_nodeview(nv.node_id());
        if (!nnv.prev().empty() or !nnv.next().empty()){
            used++;
            if(print_pos){
                if (current_node_pos.find(nv.node_id()) != current_node_pos.end())
                    std::cout << current_node_pos[nv.node_id()] << std::endl;
                if (current_node_pos.find(-nv.node_id()) != current_node_pos.end())
                    std::cout << -current_node_pos[-nv.node_id()] << std::endl;
            }
        } else {
            unused++;
            if (print_pos) std::cout << "unplaced" << std::endl;
        }

        std::vector<LinkView > lf;
        for (const auto& x: nv.next()){
            if (x.support().id == rid){
                lf.push_back(x);
            }
        }
        if (lf.empty()) break;
        nv=lf[0].node();
        if (seen.find(nv.node_id()) != seen.end()){
            return std::make_pair(0,0);
        } else {
            seen.insert(nv.node_id());
        }
    }
    return std::make_pair(used, unused);
}

std::vector<int32_t > TheGreedySorter::evaluate_read_nodeorder(uint64_t rid, bool print_pos){
    auto nv = trg_nt.get_nodeview(read_ends[rid][0]);
    auto current_node_pos = sort_graph();

    int32_t ordered = 0;
    int32_t unordered = 0;
    int32_t unused = 0;

    uint32_t last_pos = 0;
    std::unordered_set<sgNodeID_t > seen={llabs(nv.node_id())};
    // TODO: check here i'm using 0 as a None, this might be the cause of the node difference or more!!!!!! <-------
    while (true) {
        // in the original it's none, here i'm using the null node
        uint32_t p=0;
        if (current_node_pos.find(nv.node_id()) != current_node_pos.end()){
            p = current_node_pos[nv.node_id()];
        }
        else if (current_node_pos.find(-nv.node_id()) != current_node_pos.end()) {
            p = -current_node_pos[-nv.node_id()];
        }
        else{
            p = 0;
        }

        if (p==0){
            unused++;
        } else if (last_pos ==0 or last_pos<p) {
            last_pos = p;
            ordered++;
        } else {
            std::cout << "Node "<< nv.node_id() <<" is unordered in read "<< rid << std::endl;
            unordered++;
        }

        std::vector<LinkView > lf;
        for (const auto& x: nv.next()){
            if (x.support().id == rid){
                lf.push_back(x);
            }
        }
        if (lf.empty()) break;
        nv = lf[0].node();
        if (seen.find(nv.node_id())!=seen.end()){ // TODO: check the abs or not nv.node_id() if bug or not
            return std::vector<int32_t > {0, 100, 0};
        } else {
            seen.insert(nv.node_id()); // TODO: check the abs or not nv.node_id() if bug or not
        }
    }
    return {ordered, unordered, unused};
}

void TheGreedySorter::add_read(uint64_t rid, int min_confirmation){
    auto nv = trg_nt.get_nodeview(read_ends[rid][0]);
    int links_added=0;
    std::vector<sgNodeID_t > added_nodes;
    while (true) {
        added_nodes.push_back(nv.node_id());
        added_nodes.push_back(-nv.node_id());

        std::vector<LinkView > vlf;
        for (const auto& x: nv.next()){
            if (x.support().id == rid){
                vlf.push_back(x);
            }
        }
        if (vlf.empty()) break;
        LinkView lf=vlf[0];

        dg.add_link(-nv.node_id(), lf.node().node_id(), lf.distance(), lf.support());
        nv=lf.node();
        links_added++;
    }
    std::cout << links_added << " links added" << std::endl;
    // Pop all nodes that dont have support confirmations bw or fw
    bool popped = true;
    int nodes_popped = 0;
    while (popped) {
        popped = false;
        int nvs_analysed = 0;
        for (const auto& nid: added_nodes){
            nv = dg.get_nodeview(nid);
            std::vector<LinkView > lf;
            for (const auto& x: nv.next()){
                if (x.support().id == rid){
                    lf.push_back(x);
                }
            }
            if (lf.empty()) continue;
            nvs_analysed++;
            if(shared_reads(trg_nt.get_nodeview(nv.node_id()), trg_nt.get_nodeview(lf[0].node().node_id()))<=min_confirmation){
                pop_node(nv.node_id(), rid);
                popped=true;
                nodes_popped++;
            }
        }
        if (nvs_analysed<2){
            std::cout << "aborting since <2 nodes are left" << std::endl;
            break;
        }
    }
    std::cout << nodes_popped << " nodes popped" << std::endl;
    for (const auto& nv: dg.get_all_nodeviews(false, false)){
        used_nodes.push_back(nv.node_id());
    }
    used_reads.push_back(rid);
}

void TheGreedySorter::write_connected_nodes_graph(std::string filename){
    std::vector<sgNodeID_t > sn;

    for (const auto& nv: dg.get_all_nodeviews(false, false)){
        if (!dg.links[llabs(nv.node_id())].empty()){
            sn.push_back(nv.node_id());
        }
    }
    dg.write_to_gfa1(filename, sn);
}