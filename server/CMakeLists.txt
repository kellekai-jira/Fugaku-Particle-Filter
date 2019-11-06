file(GLOB
     ALL_SERVER
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     *.cxx
     )

add_executable(melissa_server ${ALL_SERVER} $<TARGET_OBJECTS:melissa_common>)

target_link_libraries(melissa_server ${FTI_DYNAMIC_LIB})
target_include_directories(melissa_server PUBLIC ${FTI_INCLUDE_DIR})

target_link_libraries(melissa_server pdaf_wrapper)

target_include_directories(melissa_server PUBLIC ${CMAKE_SOURCE_DIR}/pdaf-wrapper)

# PDAF:
target_link_libraries(melissa_server gfortran lapack blas m)

# To link against pdaf stuff:
target_link_libraries(melissa_server ${MPI_Fortran_LIBRARIES})



INSTALL(TARGETS melissa_server RUNTIME DESTINATION bin)
