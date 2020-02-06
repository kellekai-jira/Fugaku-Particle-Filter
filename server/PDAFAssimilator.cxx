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

extern int ENSEMBLE_SIZE;
extern int TOTAL_STEPS;

PDAFAssimilator::~PDAFAssimilator() {
    // call to fortran:
    cwrapper_PDAF_deallocate();
}

PDAFAssimilator::PDAFAssimilator(Field &field_)
    : field(field_) {
    // call to fortran:
    //int vect_size = field.globalVectSize();

    // as size_t might be too big for fortran...:
    //int local_vect_size = field.local_vect_size;

    // TODO: not really a changeable parameter yet. maybe the best would be to pass all parameters the pdaf style so we can reuse their parsing functions?
    assert (ENSEMBLE_SIZE <= 9);
    // we transmit only one third to pdaf
    const int pdaf_vect_size = 50*50*12;
    const int pdaf_local_vect_size = 50*50*12/2;
    cwrapper_init_pdaf(&pdaf_vect_size, &pdaf_local_vect_size, &ENSEMBLE_SIZE);
    cwrapper_init_user(&TOTAL_STEPS);
    nsteps = -1;

    // init ensemble!
    // we actually only do this herre to not confuse pdaf. PDAF want's us to start with
    // a get state phase I 'guess' ;)
    getAllEnsembleMembers();
}



void PDAFAssimilator::on_init_state(const int runner_id, const Part & part, const double * values)
{
    // We need the init state part of density and saturation as those are not inited by
    // init_ens.F90 To be sure we just copy everything, even if the pressure part
    // get's overwritten later.
    // REM: we only send the pressure part for assimilation to PDAF later.

    assert(part.send_count + part.local_offset_server <=
           field.ensemble_members[0].state_background.size());

    if (runner_id != 0)
    {
        // This does not work with more than one runner because of race conditions!
        // Only copy state from runner 0 as they are inited from the same file
        // This runner may not crash. otherwise we never start...
        // anyway. Further so we init all the ensemble members even if there are less
        // runners than ensemble members.
        return;
    }

    D("Setting all the ensemble members to the received states");

    for (size_t member_id = 0; member_id < field.ensemble_members.size(); member_id++)
    {
        // copy into all background states....
        // (here some cacheline optimization could be done by traversing the array in
        // the other way but this is not important at all as this code is executed only
        // once)
        std::copy(values, values + part.send_count,
                  field.ensemble_members[member_id].state_background.data() +
                  part.local_offset_server);

        // Also copy into analysis state to send it back right again!
        // --> start all members from the same atm.... really boring! need to change this! = FIXME
        //std::copy(values, values + part.send_count,
                  //field.ensemble_members[member_id].state_analysis.data() +
                  //part.local_offset_server);

        // this seems a really good guess for all fields except for the pressure:
        std::fill(field.ensemble_members[member_id].state_analysis.begin()+(50*50*12/2),
                  field.ensemble_members[member_id].state_analysis.end(),
                  1.0);
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
        //const int dim = eit->state_analysis.size();
        const int dim = 50*50*12/2;

        double * data = eit->state_analysis.data();
        // int nnsteps =
        int nnsteps = cwrapper_PDAF_get_state(&doexit, &dim, &data,
                                              &status);

        // copy the rest of the state from the background as we do not perform assimilaton on it.
        std::copy(eit->state_background.begin() + dim, eit->state_background.end(),
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

        //const int dim = eit->state_background.size();
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
