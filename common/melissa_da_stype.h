#ifndef __MELISSA_DA_STYPE_H__
#define __MELISSA_DA_STYPE_H__

#define VEC_T char
#define VARID_T int  // TODO: change to short in future? As I do not know for now how to do shorts in Fortran I stay with ints so far.

static_assert(alignof(VEC_T) == 1,
              "VEC_T alignment must be 1 to avoid problems with unaligned memory accesses");

// #define INDEX_MAP_T unsigned int  // TODO:  use this as index map type?!
// #define MPI_INDEX_MAP_T MPI_UNSIGNED //_INT

typedef struct index_map_s
{
    int index;
    int varid;
} index_map_t;

#define INDEX_MAP_T index_map_t
// #define MPI_INDEX_MAP_T MPI_INT






#endif
