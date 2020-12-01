//
// Created by Bernardo Clavijo (EI) on 30/11/2020.
//

#include "ReadThreadsGraph.hpp"
#include <sdglib/views/NodeView.hpp>

void ReadThreadsGraph::dump(std::string filename) {
    std::ofstream ofs(filename);
    DistanceGraph::write(ofs);
    sdglib::write_flat_unorderedmap(ofs,thread_info);
}

void ReadThreadsGraph::load(std::string filename) {
    std::ifstream ifs(filename);
    DistanceGraph::read(ifs);
    sdglib::read_flat_unorderedmap(ifs,thread_info);
}

bool ReadThreadsGraph::add_thread(int64_t thread_id, const std::vector<NodePosition> &node_positions, bool remove_duplicated, int min_thread_nodes) {
    std::unordered_set<sgNodeID_t> seen,duplicated;
    if (node_positions.size()<min_thread_nodes) return false;
    if (remove_duplicated) {
        seen.clear();
        duplicated.clear();
        for (const auto &p:node_positions){
            if (seen.count(llabs(p.node))) duplicated.insert(llabs(p.node));
            else seen.insert(llabs(p.node));
        }
    }
    uint16_t lidx=0;
    int16_t last_valid_i=-1;
    sgNodeID_t end1;
    for (auto i = 0; i < node_positions.size(); ++i) {
        if (remove_duplicated and  duplicated.count(llabs(node_positions[i].node))) continue;
        if (last_valid_i!=-1) {
            add_link(-node_positions[last_valid_i].node, node_positions[i].node,
                     node_positions[i].start - node_positions[last_valid_i].end,
                     {SupportType::LongRead, lidx++, thread_id});
            end1=node_positions[last_valid_i].node;
        }
        last_valid_i=i;
    }
    if (last_valid_i==-1) return false;
    sgNodeID_t end2=-node_positions[last_valid_i].node;
    thread_info[thread_id]={end1, end2, lidx};
    return true;
}

bool ReadThreadsGraph::remove_thread(int64_t thread_id) {
    thread_id=llabs(thread_id);
    if (thread_info.count(thread_id) == 0) return false;
    auto next_nid=thread_info[thread_id].start;
    int16_t next_link=0;
    bool link_deleted=false;
    do {
        link_deleted=false;
        for (auto nl:get_nodeview(next_nid).next()){
            if (nl.support().id==thread_id and nl.support().index==next_link) {
                remove_link(-next_nid,nl.node().node_id(),nl.distance(),nl.support());
                next_nid=nl.node().node_id();
                next_link+=1;
                link_deleted=true;
                break;
            }
        }

    } while (link_deleted);
    thread_info.erase(thread_id);
    return true;
}

NodeView ReadThreadsGraph::get_thread_start_nodeview(int64_t thread_id) {
    return get_nodeview((thread_id>0 ? thread_info[thread_id].start : thread_info[-thread_id].end));
}

NodeView ReadThreadsGraph::get_thread_end_nodeview(int64_t thread_id) {
    return get_thread_start_nodeview(-thread_id);
}