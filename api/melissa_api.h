/*
 * melissa_api.h
 *
 *  Created on: Jul 23, 2019
 *      Author: friese
 */

#ifndef API_MELISSA_API_H_
#define API_MELISSA_API_H_

#include <mpi.h>

void melissa_init(const char *field_name,
                       const int  local_vect_size,
                       MPI_Comm comm_);  // TODO do some crazy shit (dummy mpi implementation?) if we compile without mpi.


// TODO: test what happens when not acting like the following important hint! ( especially have different sleep times per rank ;)
// IMPORTANT: NEVER call melissa_expose twice without an mpi barrier in between!
bool melissa_expose(const char *field_name, double *values);


/// For debug reasons it sometimes is practical to have the melissa current state id outside of melissa.
/// returns -1 if no state is currently calculated.
int melissa_get_current_state_id();

int melissa_get_current_timestamp();

#endif /* API_MELISSA_API_H_ */
