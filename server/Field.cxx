/*
 * Field.cxx
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#include "Field.h"

Field::Field(const std::string &name_, const int simu_comm_size_, const size_t
             ensemble_size_)
    : name(name_), local_vect_size(0)
{
    local_vect_sizes_runner.resize(simu_comm_size_);
    ensemble_members.resize(ensemble_size_);
}

/// Calculates all the state vector parts that are send between the server and the
/// simulations
void Field::calculate_parts(int server_comm_size)
{
    parts = calculate_n_to_m(server_comm_size, local_vect_sizes_runner);
    for (auto part_it = parts.begin(); part_it != parts.end(); part_it++)
    {
        if (part_it->rank_server == comm_rank)
        {
            local_vect_size += part_it->send_count;
            connected_runner_ranks.emplace(part_it->rank_runner);
        }
    }

    for (auto ens_it = ensemble_members.begin(); ens_it !=
         ensemble_members.end(); ens_it++)
    {

        ens_it->set_local_vect_size(local_vect_size);          // low: better naming: local state size is in doubles not in bytes!
    }

    assert(connected_runner_ranks.size() > 0);  // if this assert is catching you probably have a field that is too big. (there are more server ranks than field elements. this makes not much sense!
    D("Calculated parts");
}

/// Finds the part of the field with the specified simu_rank.
Part & Field::getPart(int simu_rank)
{
    assert(parts.size() > 0);
    for (auto part_it = parts.begin(); part_it != parts.end(); part_it++)
    {
        if (part_it->rank_server == comm_rank && part_it->rank_runner ==
            simu_rank)
        {
            return *part_it;
        }
    }
    assert(false);     // Did not find the part!
    return *parts.end();
}

size_t Field::globalVectSize() {
    size_t res = 0;
    for (auto it = local_vect_sizes_runner.begin(); it !=
         local_vect_sizes_runner.end(); ++it)
    {
        res += *it;
    }
    return res;
}
