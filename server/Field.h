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

/**
 * About model state from a Melissa-DA perspective:
 *
 * simulation state = static state + moving state
 * moving state = hidden state + assimimlated state
 * if you want to restore a simulatoin you need to restore its state (in memory)
 * - static state: are things that do not change between members
 *   (like mesh distribution...)
 * - moving state: parts that need to be moved when mitigating one member from one
 *   runner to another thus it is saved by the server
 * - assimilated state: part that is concerned by the assimilation and thus changed
 *   during the assimilation update step
 * - hidden state: part of the moving state that is not changed by the assimilation but
 *   need to be stored by the server so it stores the full moving state.
 *
 * Nomenclatur is under discussion and thus may change in future.
 */

struct Field
{
    std::string name;
    // index: state id.
    std::vector<EnsembleMember> ensemble_members;

    size_t local_vect_size;
    std::vector<size_t> local_vect_sizes_runner;  // how the field is distributed on each runner rank
    std::vector<Part> parts;

    size_t local_vect_size_hidden;
    std::vector<size_t> local_vect_sizes_runner_hidden;  // how the hidden state is distributed on each runner rank
    std::vector<Part> parts_hidden;

    std::set<int> connected_runner_ranks;

    Field(const std::string &name, const int simu_comm_size_, const size_t
          ensemble_size_);
    void calculate_parts(int server_comm_size);

    // low: maybe inline those two getPart... functions?
    const Part & getPart(int simu_rank) const;
    const Part & getPartHidden(int simu_rank) const;

    size_t globalVectSize();
    size_t globalVectSizeHidden();

    std::vector<int> local_index_map;
    std::vector<int> local_index_map_hidden;

};

#endif /* FIELD_H_ */
