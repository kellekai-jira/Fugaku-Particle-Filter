project(Melissa LANGUAGES CXX Fortran C)
cmake_minimum_required(VERSION 3.2.2)
enable_testing()

find_package(MPI REQUIRED)
include(melissa/melissa-zmq.cmake)


# FTI #

option(WITH_FTI OFF)
option(WITH_FTI_THREADS OFF)
option(INSTALL_FTI OFF)
if(INSTALL_FTI)
    include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
    ExternalProject_Add(FTI
        GIT_REPOSITORY https://github.com/leobago/fti.git
        GIT_TAG melissa
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        PREFIX ${CMAKE_CURRENT_BINARY_DIR}/FTI
        CMAKE_ARGS -DENABLE_TESTS=0 -DENABLE_EXAMPLES=0 -DENABLE_HDF5=1
        -DHDF5_ROOT=${HDF5_ROOT}
        -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}/FTI)
    MESSAGE(STATUS "FTI will be installed")
    set(FTI_PATH "${CMAKE_INSTALL_PREFIX}/FTI")
    set(FTI_LIBRARY "${FTI_PATH}/lib/libfti.so")
    set(FTI_INCLUDE_DIR "${FTI_PATH}/include")
    set(WITH_FTI TRUE)
endif()
if(WITH_FTI OR WITH_FTI_THREADS)
    if (WITH_FTI_THREADS)
        find_package(Threads REQUIRED)
        link_libraries(${CMAKE_THREAD_LIBS_INIT})
        message("here")
    endif()
    if(EXISTS "${FTI_PATH}/lib/libfti.so")
        set(FTI_LIBRARY "${FTI_PATH}/lib/libfti.so")
    else()
        message(FATAL_ERROR "could not find FTI library in ${FTI_PATH}/lib")
    endif()
    if(EXISTS "${FTI_PATH}/include/fti.h")
        set(FTI_INCLUDE_DIR "${FTI_PATH}/include")
    else()
        message(FATAL_ERROR "could not find FTI header file in ${FTI_PATH}/include")
    endif()
    message("-- found fti (${FTI_LIBRARY})")
endif()
include_directories(SYSTEM ${MPI_INCLUDE_PATH})
add_definitions(${MPI_C_COMPILE_FLAGS})

include_directories(BEFORE ${CMAKE_SOURCE_DIR}/common)
include_directories(BEFORE ${CMAKE_SOURCE_DIR}/api)

include_directories(${ZeroMQ_INCLUDE_DIR})
link_libraries(${ZeroMQ_LIBRARY})
link_libraries(${MPI_LIBRARIES})

# For intel compilers necessary:
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lstdc++")


# for compatibility with pdaf makefile....


find_package(LAPACK REQUIRED)
find_package(BLAS REQUIRED)
SET(Fortran_MATH_LIBRARIES "${BLAS_LIBRARIES};${LAPACK_LIBRARIES}")

# need all these options (especially default integer for mkl)
#SET(CMAKE_Fortran_FLAGS "-fdefault-integer-8 -fdefault-real-8 -m64")
IF(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    SET(CMAKE_Fortran_FLAGS "-m64 -real-size 64")
ELSE()
    SET(CMAKE_Fortran_FLAGS "-fdefault-real-8 -m64")
endif()

SET(CMAKE_Fortran_FLAGS_DEBUG "${CMAKE_Fortran_FLAGS_DEBUG} -fbounds-check -O0 -fimplicit-none  -Wall  -Wline-truncation  -Wcharacter-truncation  -Wsurprising  -Waliasing  -Wimplicit-interface  -Wunused-parameter  -fwhole-file  -fcheck=all  -pedantic  -fbacktrace -Wextra")

SET(PDAF_PATH $ENV{HOME}/workspace/PDAF-D_V1.15 CACHE PATH "Path to PDAF")

#SET(Fortran_MATH_LIBRARIES "-lmkl_gf_ilp64 -lmkl_sequential -lmkl_core -lpthread -lm -ldl")

#SET(Fortran_MATH_LIBRARIES ${BLAS_LIBARAIES})


# Testing convenience:                                                      $0 in the -c environment, not a part of $@
SET(in_env sh -c ". ${CMAKE_INSTALL_PREFIX}/bin/melissa-da_set_env.sh\; $@" /bin/sh)

# Fault tolerance
option(RUNNERS_MAY_CRASH "If activated model task runners may crash. This adds a bit of MPI communication" ON)

# Profiling options:
option(REPORT_TIMING "Will collect and report timing and bandwidth information on the server side at the end" ON)
option(REPORT_MEMORY "Will collect RAM information on the server side (rank 0) every 5s and log them to stdout" OFF)

configure_file(common/melissa-da_config.h.in include/melissa-da_config.h)
include_directories ("${PROJECT_BINARY_DIR}/include")


# pdaf-core is not in the f2008 standard
#SET(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -std=f95")
add_subdirectory(pdaf-core)

#SET(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -std=f2008")
add_subdirectory(common)
add_subdirectory(pdaf-wrapper)
add_subdirectory(server)
add_subdirectory(api)

add_subdirectory(launcher)


add_subdirectory(examples)

find_package(Melissa REQUIRED HINTS "${CMAKE_SOURCE_DIR}/melissa/install/share/cmake/Melissa")

configure_file(melissa-da_set_env.sh.in melissa-da_set_env.sh @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/melissa-da_set_env.sh DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    PERMISSIONS OWNER_WRITE
                OWNER_READ
                OWNER_EXECUTE
                GROUP_READ
                GROUP_EXECUTE
                WORLD_READ
                WORLD_EXECUTE)

include(CMakePackageConfigHelpers)

set(MELISSA_CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/cmake/${PROJECT_NAME}" CACHE STRING "install path for MelissaConfig.cmake")
configure_package_config_file(${PROJECT_NAME}Config.cmake.in
                              "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                              INSTALL_DESTINATION ${MELISSA_CMAKECONFIG_INSTALL_DIR})
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
                                 VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}
                                 COMPATIBILITY AnyNewerVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
              DESTINATION ${MELISSA_CMAKECONFIG_INSTALL_DIR})

