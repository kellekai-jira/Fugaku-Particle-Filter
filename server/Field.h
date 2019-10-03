/*
 * Field.h
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#ifndef FIELD_H_
#define FIELD_H_
#include <vector>
#include <set>

#include "EnsembleMember.h"

#include "utils.h"

struct Field
{
        std::string name;
        // index: state id.
        std::vector<EnsembleMember> ensemble_members;

        size_t local_vect_size;
        std::vector<size_t> local_vect_sizes_runner;
        std::vector<Part> parts;

        std::set<int> connected_runner_ranks;

        Field(const std::string &name, const int simu_comm_size_, const size_t
              ensemble_size_);
        void calculate_parts(int server_comm_size);
        Part & getPart(int simu_rank);

        size_t globalVectSize();
};

#endif /* FIELD_H_ */
