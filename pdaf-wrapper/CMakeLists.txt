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

add_library(pdaf_wrapper STATIC ${ALL_PDAF_WRAPPER} ${ALL_USER})
target_compile_options(pdaf_wrapper BEFORE PUBLIC -fPIC)


# PDAF:
target_link_libraries(pdaf_wrapper pdaf)
target_link_libraries(pdaf_wrapper gfortran m)

# To link against pdaf stuff:
target_link_libraries(pdaf_wrapper ${MPI_Fortran_LIBRARIES} ${Fortran_MATH_LIBRARIES})

