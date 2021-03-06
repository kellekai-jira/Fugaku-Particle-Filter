/*
 * Field.cxx
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#include "Field.h"

Field::Field(const std::string &name_, const int simu_comm_size_, const size_t
          ensemble_size_, int bytes_per_element_, int bytes_per_element_hidden_)
    : name(name_), local_vect_size(0), local_vect_size_hidden(0),
    bytes_per_element(bytes_per_element_), bytes_per_element_hidden(bytes_per_element_hidden_)
{
    local_vect_sizes_runner.resize(simu_comm_size_);
    local_vect_sizes_runner_hidden.resize(simu_comm_size_);
    ensemble_members.resize(ensemble_size_);
}

/// Calculates all the state vector parts that are send between the server and the
/// simulations
void Field::calculate_parts(int server_comm_size)
{
    parts = calculate_n_to_m(server_comm_size, local_vect_sizes_runner, bytes_per_element);
    parts_hidden = calculate_n_to_m(server_comm_size,
                                    local_vect_sizes_runner_hidden,
                                    bytes_per_element_hidden);

    assert(parts_hidden.size() == 0 || parts_hidden.size() == parts.size());
    auto part_it_hidden = parts_hidden.begin();

    for (auto part_it = parts.begin(); part_it != parts.end(); part_it++)
    {
        if (part_it->rank_server == comm_rank)
        {
            local_vect_size += part_it->send_count;
            // REFACTOR: maybe we can use the exact same data structures on the api side and on the server side?
            connected_runner_ranks.emplace(part_it->rank_runner);
            if (parts_hidden.size() > 0)
            {
                // assume the same parts (just different sizes) exist for the hidden state
                assert(part_it_hidden->rank_server == comm_rank);
                local_vect_size_hidden += part_it_hidden->send_count;
            }
        }
        if (parts_hidden.size() > 0)
        {
            part_it_hidden++;
        }
    }

    for (auto ens_it = ensemble_members.begin(); ens_it !=
         ensemble_members.end(); ens_it++)
    {

        ens_it->set_local_vect_size(local_vect_size, local_vect_size_hidden);          // low: better naming: local state size is in VEC_Ts not forcibly bytes!
    }

    assert(connected_runner_ranks.size() > 0);  // if this assert is catching you probably have a field that is too small. (there are more server ranks than field elements. this makes not much sense!

    local_index_map.resize(local_vect_size / bytes_per_element);
    local_index_map_hidden.resize(local_vect_size_hidden / bytes_per_element_hidden);

    MDBG("Calculated parts");
}

/// Finds the part of the field with the specified simu_rank.
const Part & Field::getPart(int simu_rank) const
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

/// Finds the hidden part of the field with the specified simu_rank.
const Part & Field::getPartHidden(int simu_rank) const
{
    static const Part null_part {-1, 0, -1, 0, 0};
    if (parts_hidden.size() == 0)
    {
        return null_part;
    }

    for (auto part_it = parts_hidden.begin(); part_it != parts_hidden.end();
         part_it++)
    {
        if (part_it->rank_server == comm_rank && part_it->rank_runner ==
            simu_rank)
        {
            return *part_it;
        }
    }
    assert(false);     // Did not find the part!
    return *parts_hidden.end();
}

size_t Field::globalVectSize() {
    return sum_vec(local_vect_sizes_runner);
}

size_t Field::globalVectSizeHidden() {
    return sum_vec(local_vect_sizes_runner_hidden);
}
