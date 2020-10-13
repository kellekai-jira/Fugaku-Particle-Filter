project(Melissa LANGUAGES CXX Fortran C)
cmake_minimum_required(VERSION 3.10.2)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/melissa/cmake/Modules)

enable_testing()

# For intel compilers necessary:
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lstdc++")
#set(CMAKE_CXX_STANDARD 14)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MPI #
find_package(MPI REQUIRED)
include_directories(SYSTEM ${MPI_INCLUDE_PATH})
add_definitions(${MPI_C_COMPILE_FLAGS})
link_libraries(${MPI_LIBRARIES})

# FTI #
option(WITH_FTI "FTI is needed for automatic recovery from melissa_server crashes" OFF)
if (WITH_FTI)
    option(WITH_FTI_THREADS "Threads will perform server checkpoints in the background. Activation increases the performance when using FTI." OFF)
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
        message(STATUS "FTI will be installed")
        set(FTI_PATH "${CMAKE_INSTALL_PREFIX}/FTI")
        set(FTI_LIBRARY "${FTI_PATH}/lib/libfti.so")
        set(FTI_INCLUDE_DIR "${FTI_PATH}/include")
        set(WITH_FTI TRUE)
    else()
        set(FTI_PATH "" CACHE PATH "Path to FTI")
        find_path(FTI_INCLUDE_DIR NAMES fti.h PATHS ${FTI_PATH}/include)
        find_library(FTI_LIBRARY NAMES libfti.so PATHS ${FTI_PATH}/lib)
        message("-- found fti (${FTI_LIBRARY})")
    endif()

    if (WITH_FTI_THREADS)
        find_package(Threads REQUIRED)
        link_libraries(${CMAKE_THREAD_LIBS_INIT})
    endif()
    install(FILES config.fti
            DESTINATION share/melissa-da)
    # set to TRUE so even if it was ON it's TRUE now. This is easier readable by others
    # relying on the setenv script which exports this variable
    set(WITH_FTI TRUE)
else()
    set(INSTALL_FTI OFF)
    set(WITH_FTI_THREADS OFF)
endif()

# ZeroMQ #
# TODO: uncomment the following 3 lines and remove the include(melissa/melissa-zmq.cmake
#       to depend on the latest melissa version and its ZMQ version...
#list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/melissa/cmake/Modules)
#include(BuildZeroMQ)
find_package(ZeroMQ REQUIRED)
include_directories(${ZeroMQ_INCLUDE_DIR})
link_libraries(${ZeroMQ_LIBRARY})


# Melissa submodule #
find_package(Melissa REQUIRED HINTS "${CMAKE_SOURCE_DIR}/melissa/install/share/cmake/Melissa")



# for compatibility with pdaf makefile....


find_package(LAPACK REQUIRED)
find_package(BLAS REQUIRED)
set(Fortran_MATH_LIBRARIES "${BLAS_LIBRARIES};${LAPACK_LIBRARIES}")

# need all these options (especially default integer for mkl)
#set(CMAKE_Fortran_FLAGS "-fdefault-integer-8 -fdefault-real-8 -m64")
IF(CMAKE_Fortran_COMPILER_ID STREQUAL "Intel")
    set(CMAKE_Fortran_FLAGS "-m64 -real-size 64")
    set(COMPILER_DEPENDENT_FORTRAN_FLAGS "")
    # for nice traces:
    set(CMAKE_C_FLAGS_DEBUG "-g -rdynamic -traceback")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -rdynamic -traceback -std=c++14")
    set(CMAKE_Fortran_FLAGS_DEBUG "-g -rdynamic -traceback")
ELSE()
    set(CMAKE_Fortran_FLAGS "-fdefault-real-8 -m64")
    set(COMPILER_DEPENDENT_FORTRAN_FLAGS "-fwhole-file  -fcheck=all  -pedantic  -fbacktrace -Wextra -Wall  -Wline-truncation  -Wcharacter-truncation  -Wsurprising  -Waliasing  -Wimplicit-interface  -Wunused-parameter")
endif()

set(CMAKE_Fortran_FLAGS_DEBUG "-rdynamic ${CMAKE_Fortran_FLAGS_DEBUG} -fbounds-check -O0 -fimplicit-none ${COMPILER_DEPENDENT_FORTRAN_FLAGS} ")


#set(Fortran_MATH_LIBRARIES "-lmkl_gf_ilp64 -lmkl_sequential -lmkl_core -lpthread -lm -ldl")

#set(Fortran_MATH_LIBRARIES ${BLAS_LIBARAIES})



# Fault tolerance
option(RUNNERS_MAY_CRASH "If activated model task runners may crash. This adds a bit of MPI communication" ON)

# Profiling options:
option(REPORT_TIMING "Will collect and report timing and bandwidth information on the server side at the end" ON)
option(REPORT_MEMORY "Will collect RAM information on the server side (rank 0) every 5s and log them to stdout" OFF)
option(REPORT_WORKLOAD "Will print every second to stdout how many cycles server (rank 0) did in the last second" ON)
option(REPORT_TIMING_ALL_RANKS "If activated all server ranks will collect timing information and each writes a trace_melissa_server.RANK.csv filel" OFF)

# Testing convenience:                                                      $0 in the -c environment, not a part of $@
set(in_env sh -c ". ${CMAKE_INSTALL_PREFIX}/bin/melissa-da_set_env.sh\; $@" /bin/sh)



# Start doing stuff actually...
configure_file(common/melissa-da_config.h.in include/melissa-da_config.h)

include_directories(BEFORE ${CMAKE_SOURCE_DIR}/common)
include_directories(BEFORE ${CMAKE_SOURCE_DIR}/api)
include_directories ("${PROJECT_BINARY_DIR}/include")

# pdaf-core is not in the f2008 standard
#set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -std=f95")
add_subdirectory(pdaf-core)
#set(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -std=f2008")

add_subdirectory(common)
add_subdirectory(pdaf-wrapper)
add_subdirectory(server)
add_subdirectory(api)

add_subdirectory(launcher)


add_subdirectory(examples)


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

# TODO: rename the package to melissa-da see issue melissa-da#5 !
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

