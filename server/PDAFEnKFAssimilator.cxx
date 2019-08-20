/*
 * PDAFEnKFAssimilator.cxx
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#include "PDAFEnKFAssimilator.h"

#include <mpi.h>
#include <algorithm>
#include "../common/utils.h"

#include "pdaf.h"

PDAFEnKFAssimilator::~PDAFEnKFAssimilator() {
	// call to fortran:
	//PDAF_deallocate();
}

PDAFEnKFAssimilator::PDAFEnKFAssimilator() {
	// call to fortran:
	cwrapper_init_pdaf();
	//init_pdaf_();
}

// very dirty but necessary as pdaf is designed like this.
int distribute_state_ensemble_id;
std::vector<EnsembleMember> * distribute_state_ensemble_members;
void my_distribute_state (const int * dim, double * state){
	EnsembleMember & member = distribute_state_ensemble_members->at(distribute_state_ensemble_id);
	assert(*dim == member.state_analysis.size());
	std::copy(state, state + (*dim), member.state_analysis.begin());
};

// called if every state was saved.
void PDAFEnKFAssimilator::do_update_step(Field &field)
{
	int nsteps;  //    ! Number of time steps to be performed in current forecast
	int doexit;  //    ! Whether to exit forecasting (1=true)
	int status;  //    ! Status flag for filter routines
	double timenow;  //      ! Current model time

	L("Doing update step...\n");
	MPI_Barrier(MPI_COMM_WORLD);  // TODO: remove this line!



/*



	//        ! *** PDAF: Send state forecast to filter;                           ***
	//        ! *** PDAF: Perform assimilation if ensemble forecast is completed   ***
	//        ! *** PDAF: Distinct calls due to different name of analysis routine ***
	// TODO: at the moment we only support enkf!
	PDAF_put_state_enkf(collect_state, init_dim_obs, obs_op,
			init_obs, prepoststep_seik, add_obs_error,
			init_obscovar, &status);

	// TODO: do this on every ensemble member!:
	//     ! *** PDAF: Get state and forecast information (nsteps,time)  ***
	distribute_state_ensemble_members = &field.ensemble_members;
	for (distribute_state_ensemble_id = 0;
			distribute_state_ensemble_id < field.ensemble_members.size();
			distribute_state_ensemble_id++) {

		PDAF_get_state(&nsteps, &timenow, &doexit, next_observation,
				my_distribute_state, prepoststep_seik, &status);

		// for now:
		assert(nsteps == 1);

		//     ! Check whether forecast has to be performed
		if (doexit == 1 || status == 0) {
			// Something went wrong!
			D("PDAF get state wants us to exit? 1==%d", doexit);
			D("PDAF get state status=%d", status);
		}

	}
	//        ! *** Forecast ensemble states ***


	//           ! Initialize current model time
	//           time = timenow; // TODO   not needed so far but probably later...

	//           ! *** call time stepper ***
	//normally: CALL integration(time, nsteps)
	// but in melissa: done by the model task runners!
*/
}
