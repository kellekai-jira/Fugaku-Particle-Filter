#project(pdaf LANGUAGES Fortran)
#cmake_minimum_required(VERSION 3.2.2)
find_package(MPI REQUIRED)
#SET(CMAKE_Fortran_FLAGS_DEBUG "${CMAKE_Fortran_FLAGS_DEBUG} -fbounds-check -O2 -fimplicit-none  -Wall  -Wline-truncation  -Wcharacter-truncation  -Wsurprising  -Waliasing  -Wimplicit-interface  -Wunused-parameter  -fwhole-file  -fcheck=all  -std=f2008  -pedantic  -fbacktrace -Wextra")



include_directories(SYSTEM ${MPI_Fortran_INCLUDE_PATH})

add_definitions(${MPI_Fortran_COMPILE_FLAGS})

link_libraries(${MPI_Fortran_LIBRARIES})


SET(PDAF_PATH $ENV{HOME}/workspace/PDAF-D_V1.14 CACHE PATH "Path to PDAF")

file(GLOB
  ALL_PDAF
    ${PDAF_PATH}/src/*.F90
     )

add_library(pdaf STATIC ${ALL_PDAF})
target_compile_options(pdaf BEFORE PUBLIC -fPIC)


target_link_libraries(pdaf gfortran lapack blas m)

