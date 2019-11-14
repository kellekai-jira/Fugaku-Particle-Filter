project(melissa LANGUAGES CXX Fortran)
cmake_minimum_required(VERSION 3.2.2)


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

# for compatibility with pdaf makefile....
SET(CMAKE_Fortran_FLAGS "-fdefault-real-8")

SET(CMAKE_Fortran_FLAGS_DEBUG "${CMAKE_Fortran_FLAGS_DEBUG} -fbounds-check -O2 -fimplicit-none  -Wall  -Wline-truncation  -Wcharacter-truncation  -Wsurprising  -Waliasing  -Wimplicit-interface  -Wunused-parameter  -fwhole-file  -fcheck=all  -pedantic  -fbacktrace -Wextra")

SET(PDAF_PATH $ENV{HOME}/workspace/PDAF-D_V1.14 CACHE PATH "Path to PDAF")

# pdaf-core is not in the f2008 standard
add_subdirectory(pdaf-core)

SET(CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -std=f2008")
add_subdirectory(common)
add_subdirectory(pdaf-wrapper)
add_subdirectory(server)
add_subdirectory(api)
add_subdirectory(examples/simulation1)
add_subdirectory(examples/simulation2-pdaf)

configure_file(melissa-da_set_env.sh.in melissa-da_set_env.sh @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/melissa-da_set_env.sh DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    PERMISSIONS OWNER_WRITE
                OWNER_READ
                OWNER_EXECUTE
                GROUP_READ
                GROUP_EXECUTE
                WORLD_READ
                WORLD_EXECUTE)
