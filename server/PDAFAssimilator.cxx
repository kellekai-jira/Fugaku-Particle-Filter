/*
 * PDAFAssimilator.cxx
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#include "PDAFAssimilator.h"

#include <algorithm>
#include <csignal>
#include "utils.h"

#include "pdaf-wrapper.h"

PDAFAssimilator::~PDAFAssimilator() {
    // call to fortran:
    cwrapper_PDAF_deallocate();
}

PDAFAssimilator::PDAFAssimilator(Field &field_, const int total_steps, MpiManager & mpi_)
    : field(field_), mpi(mpi_) {

    // TODO: not really a changeable parameter yet. maybe the best would be to pass all parameters the pdaf style so we can reuse their parsing functions?
    assert (field.ensemble_members.size() <= 9);

    // we transmit only one third to pdaf
    // convert to fortran
    const int global_vect_size = field.globalVectSize();
    const int local_vect_size = field.local_vect_size;  // transform to int
    const int ensemble_size = field.ensemble_members.size();

    const int local_vect_size_hidden = field.local_vect_size_hidden;

    const MPI_Fint comm_world = mpi.fortranComm();
    cwrapper_init_pdaf(&global_vect_size, &local_vect_size, &ensemble_size, &comm_world);
    cwrapper_init_user(&total_steps);
    nsteps = -1;

    // init ensemble!
    // we actually only do this herre to not confuse pdaf. PDAF want's us to start with
    // a get state phase I 'guess' ;)

    //const int current_step = 0; not needed, we init at 0 already...
    //cwrapper_set_current_step(&current_step);
    getAllEnsembleMembers();
    printf("[%d] hidden state size: %d\n", comm_rank, local_vect_size_hidden);
    if (local_vect_size_hidden > 0)
    {
        for (int member_id = 0; member_id <
             ensemble_size; member_id++)
        {
            double * hidden_state_p;
            hidden_state_p =
                field.ensemble_members[member_id].state_hidden.data();



            // REM: here we are calling a function that takes the array  pointer directly and
            // does not need a pointer to it...
            cwrapper_init_ens_hidden(&local_vect_size_hidden, &ensemble_size, &member_id, hidden_state_p);
        }
    }
}

void PDAFAssimilator::getAllEnsembleMembers()
{
    int doexit;      //    ! Whether to exit forecasting (1=true)
    int status;      //    ! Status flag for filter routines
    nsteps = -1;
    // do this on every ensemble member!:
    for (auto eit = field.ensemble_members.begin(); eit !=
         field.ensemble_members.end(); eit++)
    {
        const int local_vect_size = field.local_vect_size;  // transform to int

        double * data = eit->state_analysis.data();
        // int nnsteps =
        int nnsteps = cwrapper_PDAF_get_state(&doexit, &local_vect_size, &data,
                                              &status);
        assert(nsteps == nnsteps || nsteps == -1);          // every get state should give the same nsteps!
        nsteps = nnsteps;

        //     ! Check whether forecast has to be performed
        if (doexit == 1 || status != 0)
        {
            // Something went wrong!
            D("PDAF get state wants us to exit? 1==%d", doexit);
            D("PDAF get state status=%d", status);

            nsteps = -1;
            break;
        }

    }
}

// called if every state was saved. returns nsteps, how many steps to be forcasted in the following forcast phase. returns -1 if it wants to quit.
int PDAFAssimilator::do_update_step(const int current_step)
{
    int status;      //    ! Status flag for filter routines

    MPI_Barrier(mpi.comm());      // TODO: remove this line!
    L("Doing update step...\n");

    const int local_vect_size = field.local_vect_size;  // transform to int

    //        ! *** PDAF: Send state forecast to filter;                           ***
    //        ! *** PDAF: Perform assimilation if ensemble forecast is completed   ***
    //        ! *** PDAF: Distinct calls due to different name of analysis routine ***
    // TODO: at the moment we only support estkf!

    cwrapper_set_current_step(&current_step);

    for (auto eit = field.ensemble_members.begin(); eit !=
         field.ensemble_members.end(); eit++)
    {

        const double * data = eit->state_background.data();
        cwrapper_PDAF_put_state(&local_vect_size, &data, &status);

        if (status != 0)
        {
            // Something went wrong!
            D("PDAF put state status=%d", status);
            // TODO: finish clean!
            std::raise(SIGINT);
            exit(1);
        }
    }

    //     ! *** PDAF: Get state and forecast information (nsteps,time)  ***
    getAllEnsembleMembers();
    //        ! *** Forecast ensemble states ***


    //           ! Initialize current model time
    //           time = timenow; // TODO   not needed so far but probably later...

    //           ! *** call time stepper ***
    // normally: CALL integration(time, nsteps)
    // but in melissa: done by the model task runners!
    return getNSteps();
}
