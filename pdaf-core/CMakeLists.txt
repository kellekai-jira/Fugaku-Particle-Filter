# Copyright 2019, 2020 Inria (https://www.inria.fr/)

project(pdaf LANGUAGES Fortran)
cmake_minimum_required(VERSION 3.2.2)


SET(PDAF_PATH $ENV{HOME}/workspace/PDAF-D_V1.15
    CACHE PATH "Path to PDAF source code"
)


find_package(BLAS REQUIRED)
find_package(LAPACK REQUIRED)
find_package(MPI REQUIRED)


# check if we were really given the path to the PDAF source code
if(NOT IS_DIRECTORY ${PDAF_PATH})
    message(FATAL_ERROR "PDAF root ${PDAF_PATH} is not a directory")
endif()

if(NOT EXISTS ${PDAF_PATH}/src/PDAF-D_print_version.F90)
    message(FATAL_ERROR "Cannot locate `PDAF-D_print_version.F90` in PDAF root directory")
endif()


# try to determine PDAF version
file(
    STRINGS ${PDAF_PATH}/src/PDAF-D_print_version.F90 raw_pdaf_version
    REGEX "Version [0-9]+[.][0-9]+[.][0-9]+"
)

if(NOT raw_pdaf_version)
    message(WARNING "Cannot determine PDAF version")
else()
    string(REGEX MATCH "[0-9]+[.][0-9]+[.][0-9]+" PDAF_VERSION ${raw_pdaf_version})
endif()


# Build libpdaf
file(GLOB PDAF_SOURCES ${PDAF_PATH}/src/*.F90)
add_library(pdaf SHARED ${PDAF_SOURCES})

if(PDAF_VERSION)
    set_target_properties(pdaf PROPERTIES SOVERSION ${PDAF_VERSION})
endif()

target_include_directories(
    pdaf PRIVATE
    ${BLAS_INCLUDE_DIRS}
    ${MPI_Fortran_INCLUDE_PATH}
)

target_link_libraries(
    pdaf PRIVATE
    ${BLAS_LINKER_FLAGS} ${BLAS_LIBRARIES}
    ${LAPACK_LINKER_FLAGS} ${LAPACK_LIBRARIES}
    ${MPI_Fortran_LINK_FLAGS} ${MPI_Fortran_LIBRARIES}
)


# PDAF contains a lot of functions and Melissa does not need all of them.
# We let the linker determine which ones are needed. To do this, one must
# * move every function into its own ELF (see below) section,
# * give all functions hidden visibility by default,
# * given default visibility to all functions called by Melissa, and
# * instruct the linker to perform garbage collection.
#
# The visibility is handled by the version script.
# The linker flags are specific to GNU ld. For comparison, on OS X it suffices
# to pass the flag `-dead_strip` to the linker.
#
# ELF (Executable and Linkable Format) is a standard file format for executable
# files, shared libraries, and object files on Linux systems.
target_compile_options(pdaf PRIVATE -ffunction-sections)  # TODO: this does not work for ifort
target_link_libraries(pdaf PRIVATE -Wl,--gc-sections)
target_link_libraries(pdaf PRIVATE -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libpdaf.ld)


install(TARGETS pdaf LIBRARY DESTINATION lib)
