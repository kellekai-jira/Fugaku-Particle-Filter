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


void melissa_expose(const char *field_name, double *values);




#endif /* API_MELISSA_API_H_ */
