# here we create a patch for the pdaf-wrapper. As we are using hidden state configuration
# this is necessary
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



add_library(melissa_pdaf_wrapper_parflow_pure SHARED ${ALL_PDAF_WRAPPER} ${ALL_USER} PDAFAssimilator.cxx)
target_compile_options(melissa_pdaf_wrapper_parflow_pure BEFORE PUBLIC -fPIC)

target_compile_definitions(melissa_pdaf_wrapper_parflow_pure PRIVATE -DPARFLOW_STAND_ALONE)

target_include_directories(melissa_pdaf_wrapper_parflow_pure PRIVATE ${CMAKE_SOURCE_DIR}/server)


# PDAF:
target_link_libraries(melissa_pdaf_wrapper_parflow_pure pdaf)
target_link_libraries(melissa_pdaf_wrapper_parflow_pure gfortran m)

# To link against pdaf stuff:
target_link_libraries(melissa_pdaf_wrapper_parflow_pure ${MPI_Fortran_LIBRARIES} ${Fortran_MATH_LIBRARIES})

#find_package(NetCDF REQUIRED)
find_library(NETCDF_F_LIBRARY NAMES libnetcdff.a)
if (NETCDF_F_LIBRARY-NOTFOUND)
    message(FATAL_ERROR "Cannot compile! libnetcdff.a not found!")
endif()
target_link_libraries(melissa_pdaf_wrapper_parflow_pure ${NETCDF_F_LIBRARY})

find_library(NETCDF_LIBRARY NAMES libnetcdf.so)
if (NETCDF_LIBRARY-NOTFOUND)
    message(FATAL_ERROR "Cannot compile! libnetcdf.so not found!")
endif()
target_link_libraries(melissa_pdaf_wrapper_parflow_pure ${NETCDF_LIBRARY})

find_path(NETCDF_F_INCLUDE_DIR NAMES netcdf.mod)
if (NETCDF_F_INCLUDE_DIR-NOTFOUND)
    message(FATAL_ERROR "Cannot compile! netcdf.mod not found!")
endif()
target_include_directories(melissa_pdaf_wrapper_parflow_pure PUBLIC ${NETCDF_F_INCLUDE_DIR})

install(TARGETS melissa_pdaf_wrapper_parflow_pure LIBRARY DESTINATION lib)
