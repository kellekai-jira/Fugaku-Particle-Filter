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

target_compile_definitions(pdaf_wrapper PRIVATE -DPARFLOW_STAND_ALONE)


# PDAF:
target_link_libraries(pdaf_wrapper pdaf)
target_link_libraries(pdaf_wrapper gfortran m)

# To link against pdaf stuff:
target_link_libraries(pdaf_wrapper ${MPI_Fortran_LIBRARIES} ${Fortran_MATH_LIBRARIES})

#find_package(NetCDF REQUIRED)
find_library(NETCDF_F_LIBRARY NAMES libnetcdff.a)
if (NETCDF_F_LIBRARY-NOTFOUND)
    message(FATAL_ERROR "Cannot compile! libnetcdff.a not found!")
endif()
target_link_libraries(pdaf_wrapper ${NETCDF_F_LIBRARY})

find_library(NETCDF_LIBRARY NAMES libnetcdf.so)
if (NETCDF_LIBRARY-NOTFOUND)
    message(FATAL_ERROR "Cannot compile! libnetcdf.so not found!")
endif()
target_link_libraries(pdaf_wrapper ${NETCDF_LIBRARY})

find_path(NETCDF_F_INCLUDE_DIR NAMES netcdf.mod)
if (NETCDF_F_INCLUDE_DIR-NOTFOUND)
    message(FATAL_ERROR "Cannot compile! netcdf.mod not found!")
endif()
target_include_directories(pdaf_wrapper PUBLIC ${NETCDF_F_INCLUDE_DIR})
