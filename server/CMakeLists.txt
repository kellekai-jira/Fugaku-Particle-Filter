file(GLOB
     ALL_SERVER
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     *.cxx
     )

add_executable(melissa_server ${ALL_SERVER} $<TARGET_OBJECTS:melissa_common>)

if(INSTALL_FTI)
    add_dependencies(melissa_server FTI)
endif(INSTALL_FTI)

if(WITH_FTI OR WITH_FTI_THREADS)
    target_include_directories(melissa_server PUBLIC "${FTI_INCLUDE_DIR}")
    target_link_libraries(melissa_server ${FTI_LIBRARY})
    target_compile_definitions(melissa_server PUBLIC WITH_FTI)
    find_package(HDF5 REQUIRED)
    if (NOT(HDF5_IS_PARALLEL))
        message(FATAL_ERROR "Could not find parallel HDF5 library! The one found is sequential. Please install a parallel HDF5 library!")
    endif(NOT(HDF5_IS_PARALLEL))

    target_link_libraries(melissa_server ${HDF5_LIBRARIES})
  if(WITH_FTI_THREADS)
    target_compile_definitions(melissa_server PUBLIC WITH_FTI_THREADS)
  endif(WITH_FTI_THREADS)
endif(WITH_FTI OR WITH_FTI_THREADS)

if(INSTALL_ZMQ)
  add_dependencies(melissa_server ZeroMQ)
endif(INSTALL_ZMQ)

target_link_libraries(melissa_server pdaf_wrapper)

target_include_directories(melissa_server PUBLIC ${CMAKE_SOURCE_DIR}/pdaf-wrapper)

# PDAF:
target_link_libraries(melissa_server gfortran ${Fortran_MATH_LIBRARIES} m)

# To link against pdaf stuff:
target_link_libraries(melissa_server ${MPI_Fortran_LIBRARIES})




INSTALL(TARGETS melissa_server RUNTIME DESTINATION bin)
