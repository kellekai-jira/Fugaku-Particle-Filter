/*
 * melissa_api.h
 *
 *  Created on: Jul 23, 2019
 *      Author: friese
 */

#ifndef API_MELISSA_API_H_
#define API_MELISSA_API_H_

#include <mpi.h>

#ifdef __cplusplus
extern "C" {
#endif

/// to init the hidden state if existent. Don't call if you do not need a hidden state
// hidden state size is in doubles!!
void melissa_init(const char *field_name,
                  const int local_vect_size,
                  const int local_hidden_vect_size,
                  MPI_Comm comm_
                  );       // TODO do some crazy shit (dummy mpi implementation?) if we compile without mpi.


// index map: a list of all the indicies in the order as transimitted as values in melissa_expose
// this is needed by some assimilators that use the function domainIdx() and domainIdx_hidden()
void melissa_init_with_index_map(const char *field_name,
                  const int local_vect_size,
                  const int local_hidden_vect_size,
                  MPI_Comm comm_,
                  const int local_index_map[],
                  const int local_index_map_hidden[]
                  );

// can be called from fortran or if no mpi is used (set NULL as the mpi communicator) TODO: check if null is not already used by something else!
void melissa_init_no_mpi(const char *field_name,
                         const int  *local_vect_size,
                         const int  *local_hidden_vect_size);      // comm is casted into an pointer to an mpi communicaotr if not null.

void melissa_init_f(const char *field_name,
                    int        *local_vect_size,
                    int        *local_hidden_vect_size,
                    MPI_Fint   *comm_fortran);

// TODO: test what happens when not acting like the following important hint! ( especially have different sleep times per rank ;)
// IMPORTANT: NEVER call melissa_expose twice without an mpi barrier in between!
/// returns false if simulation should end now.
int melissa_expose(const char *field_name, double *values,
                   double *hidden_values);

/// wrapper needed for the FORTRAN interface when using no hidden state as nullptr
/// transfer between FORTRAN and C is not trivial
int melissa_expose_f(const char *field_name, double *values)
{
    return melissa_expose(field_name, values, NULL);
}

/// For debug reasons it sometimes is practical to have the melissa current state id outside of melissa.
/// returns -1 if no state is currently calculated.
int melissa_get_current_state_id();

int melissa_get_current_step();


#ifdef __cplusplus
}
#endif

#endif /* API_MELISSA_API_H_ */
