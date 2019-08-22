file(GLOB
  ALL_PDAF_WRAPPER
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     *.F90
     )

add_library(pdaf_wrapper STATIC ${ALL_PDAF_WRAPPER})
target_compile_options(pdaf_wrapper BEFORE PUBLIC -fPIC)


# PDAF:
SET(PDAF_PATH /home/friese/workspace/PDAF-D_V1.13.2_melissa)
target_link_libraries(pdaf_wrapper ${PDAF_PATH}/lib/libpdaf-d.a)
target_link_libraries(pdaf_wrapper gfortran lapack blas m)

# To link against pdaf stuff:
target_link_libraries(pdaf_wrapper ${MPI_Fortran_LIBRARIES})

