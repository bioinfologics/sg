//
// Created by Bernardo Clavijo (EI) on 19/03/2018.
//

#ifndef BSG_PATHSDATASTORE_HPP
#define BSG_PATHSDATASTORE_HPP

#include <sglib/graph/SequenceGraph.hpp>
#include <sglib/Version.hpp>

/**
 * @brief a collection of paths (for instance, generated by FlowFollower or LongReadMapper)
 */
class PathsDatastore {
public:
    PathsDatastore(SequenceGraph & _sg):sg(_sg){};
    PathsDatastore& operator=(const PathsDatastore &other);

    void write(std::ofstream & output_file);
    void read(std::ifstream & input_file);
    std::vector<sgNodeID_t> origin;
    std::vector<SequenceGraphPath> paths;
    SequenceGraph & sg;
    static const bsgVersion_t min_compat;
};


#endif //BSG_PATHSDATASTORE_HPP