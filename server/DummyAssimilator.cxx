/*
 * DummyAssimilator.cxx
 *
 *  Created on: Aug 22, 2019
 *      Author: friese
 */

#include "DummyAssimilator.h"
#include <algorithm>

DummyAssimilator::DummyAssimilator(Field & field_, const int total_steps_, MpiManager & mpi_) :
    field(field_), total_steps(total_steps_), mpi(mpi_)
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

int DummyAssimilator::do_update_step(const int current_step) {
    L("Doing dummy update step...\n");
    MPI_Barrier(mpi.comm());
    int state_id = 0;
    for (auto ens_it = field.ensemble_members.begin(); ens_it !=
         field.ensemble_members.end(); ens_it++)
    {
        assert(ens_it->state_analysis.size() ==
               ens_it->state_background.size());
        for (size_t i = 0; i < ens_it->state_analysis.size(); i++)
        {
            // pretend to do some da...
            ens_it->state_analysis[i] =
                ens_it->state_background[i] + state_id;
            if (ens_it == field.ensemble_members.begin())
            {
               ens_it->state_analysis[i] += (field.ensemble_members.end()-1)->state_background[i];
            }
            else
            {
               ens_it->state_analysis[i] += (ens_it-1)->state_background[i];
            }
        }
        state_id++;
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
