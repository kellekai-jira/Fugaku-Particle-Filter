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
#include "../common/utils.h"

#include "../pdaf-wrapper/pdaf-wrapper.h"

extern int ENSEMBLE_SIZE;
extern int MAX_TIMESTAMP;

PDAFAssimilator::~PDAFAssimilator() {
	// call to fortran:
	cwrapper_PDAF_deallocate();
}

PDAFAssimilator::PDAFAssimilator(Field &field_)
	: field(field_) {
	// call to fortran:
	int vectsize = field.globalVectSize();
	// TODO: not really a changeable parameter yet. maybe the best would be to pass all parameters the pdaf style so we can reuse their parsing functions?
	assert (ENSEMBLE_SIZE == 30);
	cwrapper_init_pdaf(&vectsize, &ENSEMBLE_SIZE, &MAX_TIMESTAMP);
}

// called if every state was saved.
int PDAFAssimilator::do_update_step()
{
	int nsteps;  //    ! Number of time steps to be performed in current forecast
	int doexit;  //    ! Whether to exit forecasting (1=true)
	int status;  //    ! Status flag for filter routines
	double timenow;  //      ! Current model time

	MPI_Barrier(MPI_COMM_WORLD);  // TODO: remove this line!
	L("Doing update step...\n");


	cwrapper_assimilate_pdaf();


	return nsteps;
}
