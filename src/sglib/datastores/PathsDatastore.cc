//
// Created by Bernardo Clavijo (EI) on 19/03/2018.
//

#include "PathsDatastore.hpp"

void PathsDatastore::write(std::ofstream &output_file) {
    uint64_t count;
    count=paths.size();
    output_file.write((char *) &BSG_MAGIC, sizeof(BSG_MAGIC));
    output_file.write((char *) &BSG_VN, sizeof(BSG_VN));
    BSG_FILETYPE type(PathDS_FT);
    output_file.write((char *) &type, sizeof(type));

    output_file.write((char *)&count,sizeof(count));
    for (auto &p:paths){
        count=p.nodes.size();
        output_file.write((char *)&count,sizeof(count));
        output_file.write((char *)p.nodes.data(),count*sizeof(p.nodes[0]));
    }
    count=origin.size();
    output_file.write((char *)&count,sizeof(count));
    output_file.write((char *)origin.data(),count*sizeof(origin[0]));
}


void PathsDatastore::read(std::ifstream &input_file) {
    bsgMagic_t magic;
    bsgVersion_t version;
    BSG_FILETYPE type;
    input_file.read((char *) &magic, sizeof(magic));
    input_file.read((char *) &version, sizeof(version));
    input_file.read((char *) &type, sizeof(type));

    if (magic != BSG_MAGIC) {
        std::cerr << "This file seems to be corrupted" << std::endl;
        throw "This file appears to be corrupted";
    }

    if (version < min_compat) {
        std::cerr << "This version of the file is not compatible with the current build, please update" << std::endl;
        throw "Incompatible version";
    }

    if (type != PathDS_FT) {
        std::cerr << "This file is not compatible with this type" << std::endl;
        throw "Incompatible file type";
    }

    uint64_t count;
    input_file.read((char *)&count,sizeof(count));
    if (input_file.eof()) return;
    SequenceGraphPath p(sg);
    paths.resize(count,p);
    for (auto &p:paths){
        input_file.read((char *)&count,sizeof(count));
        p.nodes.resize(count);
        input_file.read((char *)p.nodes.data(),count*sizeof(p.nodes[0]));
    }
    input_file.read((char *)&count,sizeof(count));
    origin.resize(count);
    input_file.read((char *)origin.data(),count*sizeof(origin[0]));
}