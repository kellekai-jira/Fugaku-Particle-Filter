file(GLOB
  ALL_PDAF_SIMULATION_1
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     *.F90
     )

   add_executable(pdaf-simulation1 ${ALL_PDAF_SIMULATION_1})


# PDAF:
target_link_libraries(pdaf-simulation1 gfortran lapack blas m)

# To link against pdaf stuff:
target_link_libraries(pdaf-simulation1 ${MPI_Fortran_LIBRARIES})

target_link_libraries(pdaf-simulation1 melissa_api)


INSTALL(TARGETS pdaf-simulation1 RUNTIME DESTINATION bin)
