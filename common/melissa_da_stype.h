// Copyright (c) 2020-2021, Institut National de Recherche en Informatique et en Automatique (Inria)
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef __MELISSA_DA_STYPE_H__
#define __MELISSA_DA_STYPE_H__

#define VEC_T char
#define VARID_T int  // TODO: change to short in future? As I do not know for now how to do shorts in Fortran I stay with ints so far.

typedef double (*calculateWeightFunction)(void);

#if __cplusplus >= 201103L
static_assert(alignof(VEC_T) == 1,
              "VEC_T alignment must be 1 to avoid unaligned memory accesses");
#elif __STDC_VERSION__ >= 201112L
#include <stdalign.h>
_Static_assert(alignof(VEC_T) == 1,
               "VEC_T alignment must be 1 to avoid unaligned memory accesses");
#else
#warning "Cannot perform VEC_T type alignment check"
#endif

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
