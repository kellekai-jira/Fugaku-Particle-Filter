project(MelissaDA LANGUAGES CXX Fortran C)
cmake_minimum_required(VERSION 3.7.2)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/melissa/cmake/Modules)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)

enable_testing()

# For intel compilers necessary:
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lstdc++")
#set(CMAKE_CXX_STANDARD 14)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MPI #
find_package(MPI REQUIRED)
include_directories(${MPI_INCLUDE_PATH})
add_definitions(${MPI_C_COMPILE_FLAGS})
link_libraries(${MPI_LIBRARIES})

find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})
link_libraries(${Protobuf_LIBRARIES})

# GnuInstallDirs
include(GNUInstallDirs)

include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

# FTI #
option(WITH_FTI "FTI is needed for automatic recovery from melissa_da_server crashes" OFF)
if (WITH_FTI)
    option(WITH_FTI_THREADS "Threads will perform server checkpoints in the background. Activation increases the performance when using FTI." OFF)
    option(INSTALL_FTI OFF)
    if(INSTALL_FTI)
        ExternalProject_Add(FTI
            GIT_REPOSITORY https://gitlab.bsc.es/kkeller/fti.git
            GIT_TAG melissa-p2p
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
            PREFIX ${CMAKE_CURRENT_BINARY_DIR}/FTI
            CMAKE_ARGS -DENABLE_TESTS=0 -DENABLE_EXAMPLES=0 -DENABLE_HDF5=0
            -DHDF5_ROOT=${HDF5_ROOT} -DCMAKE_C_FLAGS=-fPIC -DCMAKE_CXX_FLAGS=-fPIC -DCMAKE_Fortran_FLAGS=fPIC
            -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}/FTI
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DMPI_CXX_COMPILER=mpiFCC -DMPI_C_COMPILER=mpifcc)
        message(STATUS "FTI will be installed")
        set(FTI_PATH "${CMAKE_INSTALL_PREFIX}/FTI")
        set(FTI_LIBRARY "${FTI_PATH}/${CMAKE_INSTALL_LIBDIR}/libfti.so")  # TIODO: use gnu package to decide automatically between lib64 and lib
        set(FTI_INCLUDE_DIR "${FTI_PATH}/include")
        set(WITH_FTI TRUE)
    else()
        set(FTI_PATH "" CACHE PATH "Path to FTI")
        find_path(FTI_INCLUDE_DIR NAMES fti.h PATHS ${FTI_PATH}/include)
        find_library(FTI_LIBRARY NAMES libfti.so PATHS ${FTI_PATH}/lib)
    endif()

    if (WITH_FTI_THREADS)
        find_package(Threads REQUIRED)
        link_libraries(${CMAKE_THREAD_LIBS_INIT})
    endif()
    # set to TRUE so even if it was ON it's TRUE now. This is easier readable by others
    # relying on the setenv script which exports this variable
    set(WITH_FTI TRUE)
    set(WITH_FTI_CHECKOINT_DA_SERVER TRUE)
else()
    set(INSTALL_FTI OFF)
    set(WITH_FTI_THREADS OFF)
    set(WITH_FTI_CHECKOINT_DA_SERVER FALSE)
endif()


# Boost headers only for udp

option(INSTALL_BOOST "Download and install a good version of boost" OFF)
if(INSTALL_BOOST)
    message("installing boost")
    ExternalProject_Add(BOOST
        URL https://dl.bintray.com/boostorg/release/1.64.0/source/boost_1_64_0.zip
        #PATCH_COMMAND ""

        PREFIX "${CMAKE_CURRENT_BINARY_DIR}/BOOST"

        #SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/Boost-src

        DOWNLOAD_NO_EXTRACT TRUE

        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND unzip "${CMAKE_CURRENT_BINARY_DIR}/BOOST/src/boost_1_64_0.zip"
          "boost_1_64_0/boost/*" -d install


        TEST_COMMAND ""

        LOG_DOWNLOAD 1
        LOG_CONFIGURE 1
        LOG_INSTALL 1
        )
    set(Boost_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}/BOOST/src/BOOST-build/install/boost_1_64_0/")
    message("Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
else()
    find_package(Boost COMPONENTS system filesystem)
endif()



# for p2p ther is no server checkpointing!
set(WITH_FTI_CHECKOINT_DA_SERVER FALSE)

# ZeroMQ #
include(BuildZeroMQ)
find_package(ZeroMQ REQUIRED)
include_directories(${ZeroMQ_INCLUDE_DIR})
link_libraries(${ZeroMQ_LIBRARY})


# depend on python 3 where unbuffered stdout and stderr can be easily configured!
find_package(PythonInterp 3 REQUIRED)
find_package(PythonLibs 3 REQUIRED)
find_package(NumPy REQUIRED)

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
    set(CMAKE_Fortran_FLAGS_DEBUG "-g -rdynamic -traceback -check all")
ELSE()
    #set(CMAKE_Fortran_FLAGS "-fdefault-real-8 -m64")
    set(CMAKE_Fortran_FLAGS "-CcdRR8 -m64")
    set(COMPILER_DEPENDENT_FORTRAN_FLAGS "-fwhole-file  -fcheck=all  -pedantic  -fbacktrace -Wextra -Wall  -Wline-truncation  -Wcharacter-truncation  -Wsurprising  -Waliasing  -Wimplicit-interface  -Wunused-parameter")
endif()

set(CMAKE_Fortran_FLAGS_DEBUG "-rdynamic ${CMAKE_Fortran_FLAGS_DEBUG} -fbounds-check -O0 -fimplicit-none ${COMPILER_DEPENDENT_FORTRAN_FLAGS} ")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_GLIBCXX_DEBUG")


#set(Fortran_MATH_LIBRARIES "-lmkl_gf_ilp64 -lmkl_sequential -lmkl_core -lpthread -lm -ldl")

#set(Fortran_MATH_LIBRARIES ${BLAS_LIBARAIES})



# Fault tolerance
option(RUNNERS_MAY_CRASH "If activated model task runners may crash. This adds a bit of MPI communication" ON)

# Profiling options:
option(REPORT_TIMING "Will collect and report timing and bandwidth information on the server side at the end" ON)
option(REPORT_MEMORY "Will collect RAM information on the server side (rank 0) every 5s and log them to stdout" OFF)
option(REPORT_WORKLOAD "Will print every second to stdout how many cycles server (rank 0) did in the last second" ON)
option(REPORT_TIMING_ALL_RANKS "If activated all server ranks will collect timing information and each writes a trace_melissa_da_server.RANK.csv filel" OFF)

# Trick MPI
option(SLOW_MPI "Activate mpiscatter and gather through files for arrays with element_count > INTMAX" OFF)
SET(SLOW_MPI_DIR "/tmp" CACHE PATH "A folder that every server rank can access. Default is /tmp which won't work on e.g. juwels")


# Testing convenience:                                                      $0 in the -c environment, not a part of $@
set(in_env sh -c ". ${CMAKE_INSTALL_PREFIX}/bin/melissa_da_set_env.sh\; $@" /bin/sh)


function(clean_up_test)
    execute_process(COMMAND bash ${CMAKE_SOURCE_DIR}/test/melissa-da-cleanup-local.sh)
endfunction()

# Start doing stuff actually...
configure_file(common/melissa_da_config.h.in include/melissa_da_config.h)

include_directories(BEFORE ${CMAKE_SOURCE_DIR}/common)
include_directories(BEFORE ${CMAKE_SOURCE_DIR}/api)
include_directories ("${PROJECT_BINARY_DIR}/include")

include_directories(${CMAKE_BINARY_DIR}/common/protocol)

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

add_subdirectory(test)


set(DATADIR_INSTALL "${CMAKE_INSTALL_PREFIX}/share/melissa-da")
add_subdirectory(share)


configure_file(melissa_da_set_env.sh.in melissa_da_set_env.sh @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/melissa_da_set_env.sh DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    PERMISSIONS OWNER_WRITE
                OWNER_READ
                OWNER_EXECUTE
                GROUP_READ
                GROUP_EXECUTE
                WORLD_READ
                WORLD_EXECUTE)

include(CMakePackageConfigHelpers)

set(MELISSADA_CMAKECONFIG_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/share/cmake/${PROJECT_NAME}" CACHE STRING "install path for MelissaDAConfig.cmake")
configure_package_config_file(${PROJECT_NAME}Config.cmake.in
                              "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                              INSTALL_DESTINATION ${MELISSADA_CMAKECONFIG_INSTALL_DIR})
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
                                 VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}
                                 COMPATIBILITY AnyNewerVersion)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
              DESTINATION ${MELISSADA_CMAKECONFIG_INSTALL_DIR})

