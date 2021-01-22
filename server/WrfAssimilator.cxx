/*
 * WrfAssimilator.cpp
 *
 *  Created on: Dec 8, 2020
 *      Author: friese
 */

#include "WrfAssimilator.h"
#include <algorithm>

WrfAssimilator::WrfAssimilator(Field & field_, const int total_steps_, MpiManager & mpi_) :
    field(field_), total_steps(total_steps_), mpi(mpi_)
{
    nsteps = 1;
}

int WrfAssimilator::do_update_step(const int current_step) {
    L("Doing empty update step...\n");
    MPI_Barrier(mpi.comm());


    // FIXME: extract some data from the index map here and print it appropriate...

    std::set<int> varids;
    for (const auto &e : field.local_index_map) {
        varids.emplace(e.varid);
    }

    for (const auto &e : field.local_index_map_hidden) {
        varids.emplace(e.varid);
    }

    D("Different varids: %lu", varids.size());

    for (auto & ens_it : field.ensemble_members)
    {
        std::copy(ens_it.state_background.begin(), ens_it.state_background.end(),
                ens_it.state_analysis.begin());
    }

    if (current_step >= total_steps)
    {
        return -1;
    }
    else
    {
        return getNSteps();
    }
}

void WrfAssimilator::on_init_state(const int runner_id, const
                                              Part & part, const
                                              VEC_T * values, const
                                              Part & hidden_part,
                                              const VEC_T * values_hidden)
{
    // let's use this to set the init.
    // you may not have more runners than ensemble members here! Otherwise some
    // would stay uninitialized!
    assert(static_cast<size_t>(runner_id) < field.ensemble_members.size());
    assert(runner_id == 0);
    // For nnow we copy the first received ensemble state everywhere.... I know this is a rather stupid way to init the ensemble!
    // TODO: later we should at least perturb all members a bit using the index map and so on...
    if (runner_id == 0) {
        for (auto & member : field.ensemble_members) {
            member.store_background_state_part(part,
                                        values, hidden_part, values_hidden);

            assert(part.send_count + part.local_offset_server <=
                   member.state_background.size());

            // copy into analysis state to send it back right again!
            std::copy(values, values + part.send_count,
                      member.state_analysis.data() +
                      part.local_offset_server);

            // copy into field's hidden state to send it back right again!
            std::copy(values_hidden, values_hidden + hidden_part.send_count,
                      member.state_hidden.data() +
                      hidden_part.local_offset_server);
        }
    }
}
