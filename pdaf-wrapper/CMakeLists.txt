file(GLOB
  ALL_PDAF_WRAPPER
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     *.F90
     )

file(GLOB
  ALL_USER
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     user/*.F90
     )

#add_library(pdaf_wrapper STATIC ${ALL_PDAF_WRAPPER} ${ALL_USER})
add_library(pdaf_wrapper SHARED ${ALL_PDAF_WRAPPER} ${ALL_USER})
target_include_directories(pdaf_wrapper PRIVATE ${CMAKE_SOURCE_DIR}/server)
target_compile_options(pdaf_wrapper BEFORE PUBLIC -fPIC)


# PDAF:
target_link_libraries(pdaf_wrapper PUBLIC pdaf)
target_link_libraries(pdaf_wrapper PUBLIC gfortran m)

# To link against pdaf stuff:
target_link_libraries(pdaf_wrapper PUBLIC ${MPI_Fortran_LIBRARIES} ${Fortran_MATH_LIBRARIES})

install (TARGETS pdaf_wrapper LIBRARY DESTINATION lib)

