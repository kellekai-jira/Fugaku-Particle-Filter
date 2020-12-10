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
        double * as_double = reinterpret_cast<double*>(ens_it->state_analysis.data());
        size_t len_double = ens_it->state_analysis.size()/sizeof(double);
        std::fill(as_double, as_double + len_double, 0.0);
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


        double * as_double = reinterpret_cast<double*>(ens_it->state_analysis.data());
        size_t len_double = ens_it->state_analysis.size()/sizeof(double);

        double * as_double_bg = reinterpret_cast<double*>(ens_it->state_background.data());

        double * as_double_bg_neighbor;
        if (ens_it == field.ensemble_members.begin())
        {
            as_double_bg_neighbor = reinterpret_cast<double*>((field.ensemble_members.end()-1)->state_background.data());
        }
        else
        {
            as_double_bg_neighbor = reinterpret_cast<double*>((ens_it-1)->state_background.data());
        }


        for (size_t i = 0; i < len_double; i++)
        {
            // pretend to do some da...
            as_double[i] = as_double_bg[i] + state_id;
            as_double[i] += as_double_bg_neighbor[i];
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
