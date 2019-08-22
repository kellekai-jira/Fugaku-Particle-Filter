file(GLOB
     ALL_SERVER
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     *.cxx
     )

add_executable(melissa_server ${ALL_SERVER} $<TARGET_OBJECTS:melissa_common> )
message("asll server: ${ALL_SERVER}")

# PDAF:
SET(PDAF_PATH /home/friese/workspace/PDAF-D_V1.13.2_melissa)
target_link_libraries(melissa_server ${PDAF_PATH}/testsuite/src/lorenz96/lorenz96_pdaf.a)
target_link_libraries(melissa_server ${PDAF_PATH}/testsuite/src/main/driver.a)
target_link_libraries(melissa_server ${PDAF_PATH}/lib/libpdaf-d.a)
target_link_libraries(melissa_server gfortran netcdf netcdff lapack blas m)

# To link against pdaf stuff:
target_link_libraries(melissa_server ${MPI_Fortran_LIBRARIES})



INSTALL(TARGETS melissa_server RUNTIME DESTINATION bin)
