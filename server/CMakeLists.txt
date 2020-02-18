file(GLOB
     ALL_SERVER
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     *.cxx
     )

add_executable(melissa_server ${ALL_SERVER} $<TARGET_OBJECTS:melissa_common>)

if(INSTALL_ZMQ)
  add_dependencies(melissa_server ZeroMQ)
endif(INSTALL_ZMQ)


target_include_directories(melissa_server PUBLIC ${CMAKE_SOURCE_DIR}/pdaf-wrapper)

# PDAF:
target_link_libraries(melissa_server gfortran ${Fortran_MATH_LIBRARIES} m)

# To link against pdaf stuff:
target_link_libraries(melissa_server ${MPI_Fortran_LIBRARIES})
# PDAF:

target_link_libraries(melissa_server pdaf)
target_link_libraries(melissa_server gfortran m)

# To link against pdaf stuff:
target_link_libraries(melissa_server ${MPI_Fortran_LIBRARIES} ${Fortran_MATH_LIBRARIES})


target_link_libraries(melissa_server pdaf_wrapper)

INSTALL(TARGETS melissa_server RUNTIME DESTINATION bin)
