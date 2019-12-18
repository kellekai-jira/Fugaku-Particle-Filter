file(GLOB
  ALL_SIMULATION_2_PDAF
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     *.F90
     )

   add_executable(simulation2-pdaf ${ALL_SIMULATION_2_PDAF})


# PDAF:
target_link_libraries(simulation2-pdaf gfortran ${Fortran_MATH_LIBRARIES} m)

# To link against pdaf stuff:
target_link_libraries(simulation2-pdaf ${MPI_Fortran_LIBRARIES})

target_link_libraries(simulation2-pdaf melissa_api)


INSTALL(TARGETS simulation2-pdaf RUNTIME DESTINATION bin)
