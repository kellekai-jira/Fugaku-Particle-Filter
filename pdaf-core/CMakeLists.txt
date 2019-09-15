#project(pdaf LANGUAGES Fortran)
#cmake_minimum_required(VERSION 3.2.2)
find_package(MPI REQUIRED)



include_directories(SYSTEM ${MPI_Fortran_INCLUDE_PATH})

add_definitions(${MPI_Fortran_COMPILE_FLAGS})

link_libraries(${MPI_Fortran_LIBRARIES})



file(GLOB
  ALL_PDAF
    ${PDAF_PATH}/src/*.F90
     )

add_library(pdaf STATIC ${ALL_PDAF})
target_compile_options(pdaf BEFORE PUBLIC -fPIC)


target_link_libraries(pdaf gfortran lapack blas m)

