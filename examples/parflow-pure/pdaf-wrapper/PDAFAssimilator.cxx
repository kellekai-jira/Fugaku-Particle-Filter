/*
 * PDAFAssimilator.cxx
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#include "PDAFAssimilator.h"

#include <mpi.h>
#include <algorithm>
#include <csignal>
#include "utils.h"

#include "pdaf-wrapper.h"

PDAFAssimilator::~PDAFAssimilator() {
    // call to fortran:
    cwrapper_PDAF_deallocate();
}

PDAFAssimilator::PDAFAssimilator(Field &field_, const int total_steps)
    : field(field_) {
    L("Using the patched assimilator for parflow-pure!");
    // call to fortran:
    // int vect_size = field.globalVectSize();

    // as size_t might be too big for fortran...:
    // int local_vect_size = field.local_vect_size;

    // TODO: not really a changeable parameter yet. maybe the best would be to pass all parameters the pdaf style so we can reuse their parsing functions?
    assert (field.ensemble_members.size() <= 9);
    // we transmit only one third to pdaf
    const int pdaf_vect_size = 50*50*12;
    const int pdaf_local_vect_size = 50*50*12/2;
    const int ensemble_size = field.ensemble_members.size();
    cwrapper_init_pdaf(&pdaf_vect_size, &pdaf_local_vect_size, &ensemble_size);
    cwrapper_init_user(&total_steps);
    nsteps = -1;

    // init ensemble!
    // we actually only do this herre to not confuse pdaf. PDAF want's us to start with
    // a get state phase I 'guess' ;)
    getAllEnsembleMembers();
    for (int member_id = 0; member_id <
         static_cast<int>(field.ensemble_members.size()); member_id++)
    {
        const int dim_p = 50*50*12/2 * 2; // saturation + dens
        const int dim_ens = field.ensemble_members.size();
        double * hidden_state_p;
        hidden_state_p =
            field.ensemble_members[member_id].state_analysis.data() + (50*50*12/
                                                                       2);

        cwrapper_init_ens_hidden(&dim_p, &dim_ens, &member_id, hidden_state_p);
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
        // const int dim = eit->state_analysis.size();
        const int dim = 50*50*12/2;

        double * data = eit->state_analysis.data();
        // int nnsteps =
        int nnsteps = cwrapper_PDAF_get_state(&doexit, &dim, &data,
                                              &status);

        // copy the rest of the state from the background as we do not perform assimilaton on it.
        std::copy(eit->state_background.begin() + dim,
                  eit->state_background.end(),
                  eit->state_analysis.begin() + dim);

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
int PDAFAssimilator::do_update_step()
{
    int status;      //    ! Status flag for filter routines

    MPI_Barrier(MPI_COMM_WORLD);      // TODO: remove this line!
    L("Doing update step...\n");


    //        ! *** PDAF: Send state forecast to filter;                           ***
    //        ! *** PDAF: Perform assimilation if ensemble forecast is completed   ***
    //        ! *** PDAF: Distinct calls due to different name of analysis routine ***
    // TODO: at the moment we only support estkf!
    for (auto eit = field.ensemble_members.begin(); eit !=
         field.ensemble_members.end(); eit++)
    {

        // const int dim = eit->state_background.size();
        const int dim  = 50*50*12/2;
        const double * data = eit->state_background.data();
        cwrapper_PDFA_put_state(&dim, &data, &status);

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
