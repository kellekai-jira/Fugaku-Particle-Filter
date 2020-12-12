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

    // otherwise release mode will make problems!
    for (auto ens_it = field.ensemble_members.begin(); ens_it !=
         field.ensemble_members.end(); ens_it++)
    {
        // analysis state is enough:
        std::fill(ens_it->state_analysis.begin(),
                  ens_it->state_analysis.end(), 290.0);  // Kelvin
    }

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

    if (current_step >= total_steps)
    {
        return -1;
    }
    else
    {
        return getNSteps();
    }
}
