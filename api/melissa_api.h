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

/// Every rank that contains some model state information and thus shall communicate with
/// the Melissa-DA server as must call this function once during init.
/// local_vect_size and local_hidden_vect_Size are in doubles.
/// local_hidden_vect_size may be 0.
// TODO: check if local_vect_size may be 0 too (it might be thinkable that some ranks have
//       no assimilated but only hidden state)
void melissa_init(const char *field_name,
                  const int local_vect_size,
                  const int local_hidden_vect_size,
                  MPI_Comm comm_
                  );


/// index map: a list of all the indicies in the order as transimitted as values in melissa_expos/
/// this is needed by some assimilators that use the function domainIdx() and domainIdx_hidden()
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
/// IMPORTANT: NEVER call melissa_expose twice without an mpi barrier in between!
/// Exposes data to melissa
/// returns 0 if simulation should end now.
/// otherwise returns nsteps, the number of timesteps that need to be simulated.
int melissa_expose(const char *field_name, double *values,
                   double *hidden_values);

/// wrapper needed for the Fortran interface when using no hidden state as nullptr
/// transfer between Fortran and C is not trivial
int melissa_expose_f(const char *field_name, double *values);

/// For debug reasons it sometimes is useful to have the melissa current state id
/// outside of melissa.
/// returns -1 if no state is currently calculated.
int melissa_get_current_state_id();

/// Get the current step that is shall be propagated. Step counting is as performed by
/// the Melissa-DA server.
int melissa_get_current_step();

/// Chunk stuff
/// TODO: write doxygen!
int melissa_commit_chunks_f(MPI_Fint * comm_fortran);

#define add_chunk_wrapper_decl(TYPELETTER, CTYPE) \
    void melissa_add_chunk_##TYPELETTER(CTYPE * values, const int * amount, \
            const int * is_assimilated)

    add_chunk_wrapper_decl(r, float);
    add_chunk_wrapper_decl(i, int);
    add_chunk_wrapper_decl(d, double);
    add_chunk_wrapper_decl(l, bool);
    add_chunk_wrapper_decl(c, char);

#undef add_chunk_wrapper_decl

#ifdef __cplusplus
}
#endif

#endif /* API_MELISSA_API_H_ */
