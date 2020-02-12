/*
 * EmptyAssimilator.cpp
 *
 *  Created on: Nov 18, 2019
 *      Author: friese
 */

#include "EmptyAssimilator.h"
#include <algorithm>

EmptyAssimilator::EmptyAssimilator(Field & field_, const int total_steps_) :
    field(field_), total_steps(total_steps_)
{
    nsteps = 1;

    // otherwise release mode will make problems!
    for (auto ens_it = field.ensemble_members.begin(); ens_it !=
         field.ensemble_members.end(); ens_it++)
    {
        // analysis state is enough:
        std::fill(ens_it->state_analysis.begin(),
                  ens_it->state_analysis.end(), 0.0);
    }

}

int EmptyAssimilator::do_update_step() {
    L("Doing empty update step...\n");
    MPI_Barrier(MPI_COMM_WORLD);

    step++;
    if (step >= total_steps)
    {
        return -1;
    }
    else
    {
        return getNSteps();
    }
}
