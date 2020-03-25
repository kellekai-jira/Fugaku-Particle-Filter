project(Melissa LANGUAGES CXX Fortran C)
cmake_minimum_required(VERSION 3.2.2)
enable_testing()

find_package(MPI REQUIRED)
# ZeroMQ #

option(INSTALL_ZMQ OFF)
if(INSTALL_ZMQ)
  include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)
  ExternalProject_Add(ZeroMQ
  URL https://github.com/zeromq/libzmq/releases/download/v4.3.1/zeromq-4.3.1.tar.gz
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/ZeroMQ
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
             -DCMAKE_INSTALL_LIBDIR:PATH=${CMAKE_INSTALL_PREFIX}/lib)
  set(ZeroMQ_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include")
  set(ZeroMQ_LIBRARY "${CMAKE_INSTALL_PREFIX}/lib/${CMAKE_SHARED_LIBRARY_PREFIX}zmq${CMAKE_SHARED_LIBRARY_SUFFIX}" )
  MESSAGE(STATUS "ZeroMQ will be installed")
else(INSTALL_ZMQ)
  find_package(ZeroMQ NO_MODULE REQUIRED)
endif(INSTALL_ZMQ)

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

