/*
 * PDAFEnKFAssimilator.cxx
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#include "PDAFEnKFAssimilator.h"

#include <mpi.h>
#include <algorithm>
#include <csignal>
#include "../common/utils.h"

#include "pdaf.h"

PDAFEnKFAssimilator::~PDAFEnKFAssimilator() {
	// call to fortran:
	cwrapper_PDAF_deallocate();
}

PDAFEnKFAssimilator::PDAFEnKFAssimilator(const int dim_state) {
	// call to fortran:
	cwrapper_init_pdaf(&dim_state);
}

// called if every state was saved.
void PDAFEnKFAssimilator::do_update_step(Field &field)
{
	int nsteps;  //    ! Number of time steps to be performed in current forecast
	int doexit;  //    ! Whether to exit forecasting (1=true)
	int status;  //    ! Status flag for filter routines
	double timenow;  //      ! Current model time

	L("Doing update step...\n");
	MPI_Barrier(MPI_COMM_WORLD);  // TODO: remove this line!








	//        ! *** PDAF: Send state forecast to filter;                           ***
	//        ! *** PDAF: Perform assimilation if ensemble forecast is completed   ***
	//        ! *** PDAF: Distinct calls due to different name of analysis routine ***
	// TODO: at the moment we only support enkf!

	for (auto eit = field.ensemble_members.begin(); eit != field.ensemble_members.end(); eit++) {
	cwrapper_PDFA_put_state(eit->state_background.data(), &status);
		if (status == 0) {
			// Something went wrong!
			D("PDAF put state status=%d", status);
			std::raise(SIGINT);
			exit(1);
		}
	}

	//     ! *** PDAF: Get state and forecast information (nsteps,time)  ***
	// do this on every ensemble member!:
	for (auto eit = field.ensemble_members.begin(); eit != field.ensemble_members.end(); eit++) {
		cwrapper_PDAF_get_state(&doexit, eit->state_analysis.data(), &status);

		//     ! Check whether forecast has to be performed
		if (doexit == 1 || status == 0) {
			// Something went wrong!
			D("PDAF get state wants us to exit? 1==%d", doexit);
			D("PDAF get state status=%d", status);
			std::raise(SIGINT);
			exit(1);
		}

	}
	//        ! *** Forecast ensemble states ***


	//           ! Initialize current model time
	//           time = timenow; // TODO   not needed so far but probably later...

	//           ! *** call time stepper ***
	//normally: CALL integration(time, nsteps)
	// but in melissa: done by the model task runners!

}
