/*
 * PDAFEnKFAssimilator.cxx
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#include "PDAFAssimilator.h"

#include <mpi.h>
#include <algorithm>
#include <csignal>
#include "utils.h"
#include <fti.h>

#include "pdaf-wrapper.h"

extern int ENSEMBLE_SIZE;
extern int TOTAL_STEPS;

PDAFAssimilator::~PDAFAssimilator() {
    // call to fortran:
    cwrapper_PDAF_deallocate();
}

PDAFAssimilator::PDAFAssimilator(Field &field_)
    : field(field_) {
    // call to fortran:
    int vectsize = field.globalVectSize();

    // ass size_t might be too big...:
    int local_vect_size = field.local_vect_size;

    // TODO: not really a changeable parameter yet. maybe the best would be to pass all parameters the pdaf style so we can reuse their parsing functions?
    assert (ENSEMBLE_SIZE <= 9);
    cwrapper_init_pdaf(&vectsize, &local_vect_size, &ENSEMBLE_SIZE);
    cwrapper_init_user(&TOTAL_STEPS);
    nsteps = -1;

    // init ensemble!
    getAllEnsembleMembers();
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
        const int dim = eit->state_analysis.size();
        double * data = eit->state_analysis.data();
        // int nnsteps =
        int nnsteps = cwrapper_PDAF_get_state(&doexit, &dim, &data,
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
int PDAFAssimilator::do_update_step()
{
    int status;      //    ! Status flag for filter routines

    MPI_Barrier(FTI_COMM_WORLD);      // TODO: remove this line!
    L("Doing update step...\n");


    //        ! *** PDAF: Send state forecast to filter;                           ***
    //        ! *** PDAF: Perform assimilation if ensemble forecast is completed   ***
    //        ! *** PDAF: Distinct calls due to different name of analysis routine ***
    // TODO: at the moment we only support estkf!
    for (auto eit = field.ensemble_members.begin(); eit !=
         field.ensemble_members.end(); eit++)
    {

        const int dim = eit->state_background.size();
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
