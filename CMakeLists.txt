project(melissa LANGUAGES CXX Fortran)
cmake_minimum_required(VERSION 3.2.2)


find_package(MPI REQUIRED)
find_package(ZeroMQ REQUIRED)

include_directories(SYSTEM ${MPI_INCLUDE_PATH})
add_definitions(${MPI_C_COMPILE_FLAGS})

include_directories(BEFORE ${CMAKE_SOURCE_DIR}/api)
include_directories(${ZeroMQ_INCLUDE_DIR})
link_libraries(${ZeroMQ_LIBRARY})
link_libraries(${MPI_LIBRARIES})

# for compatibility with pdaf makefile....
SET(CMAKE_Fortran_FLAGS "-fdefault-real-8")

SET(CMAKE_Fortran_FLAGS_DEBUG "${CMAKE_Fortran_FLAGS_DEBUG} -fbounds-check -O2 -fimplicit-none  -Wall  -Wline-truncation  -Wcharacter-truncation  -Wsurprising  -Waliasing  -Wimplicit-interface  -Wunused-parameter  -fwhole-file  -fcheck=all  -std=f2008  -pedantic  -fbacktrace -Wextra")

add_subdirectory(common)
add_subdirectory(pdaf-wrapper)
add_subdirectory(server)
add_subdirectory(api)
add_subdirectory(example-simulation)
add_subdirectory(pdaf-simulation1)

configure_file(melissa-da_set_env.sh.in melissa-da_set_env.sh @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/melissa-da_set_env.sh DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    PERMISSIONS OWNER_WRITE
                OWNER_READ
                OWNER_EXECUTE
                GROUP_READ
                GROUP_EXECUTE
                WORLD_READ
                WORLD_EXECUTE)
