/*
 * DummyAssimilator.cpp
 *
 *  Created on: Aug 22, 2019
 *      Author: friese
 */

#include "DummyAssimilator.h"
#include <algorithm>
#include "MpiManager.h"

DummyAssimilator::DummyAssimilator(Field & field_) :
    field(field_)
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

int DummyAssimilator::do_update_step() {
    L("Doing dummy update step...\n");
    MPI_Barrier(mpi().comm());
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
        }
        state_id++;
    }

    return getNSteps();
}
