/*
 * PDAFEnKFAssimilator.cxx
 *
 *  Created on: Aug 14, 2019
 *      Author: friese
 */

#include "PDAFEnKFAssimilator.h"

#include <mpi.h>
#include "../common/utils.h"

PDAFEnKFAssimilator::~PDAFEnKFAssimilator() {
	// call to fortran:
  //PDAF_deallocate();
}

PDAFEnKFAssimilator::PDAFEnKFAssimilator() {
  // call to fortran:
	//init_pdaf();
}

// called if every state was saved.
void PDAFEnKFAssimilator::do_update_step(Field &field)
{
	// TODO
	L("Doing update step...\n");
	MPI_Barrier(MPI_COMM_WORLD);
	int state_id = 0;
	for (auto ens_it = field.ensemble_members.begin(); ens_it != field.ensemble_members.end(); ens_it++)
	{
		assert(ens_it->state_analysis.size() == ens_it->state_background.size());
		for (size_t i = 0; i < ens_it->state_analysis.size(); i++) {
			// pretend to do some da...
			ens_it->state_analysis[i] = ens_it->state_background[i] + state_id;
		}
		state_id ++;
	}
}

