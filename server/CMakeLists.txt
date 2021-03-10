add_executable(melissa_da_server
    Assimilator.cxx
    CheckStatelessAssimilator.cxx
    DummyAssimilator.cxx
    EmptyAssimilator.cxx
    EnsembleMember.cxx
    FTmodule.cxx
    FTthreadManager.cxx
    Field.cxx
    LauncherConnection.cxx
    MpiManager.cxx
    PDAFAssimilator.cxx
    PrintIndexMapAssimilator.cxx
    PythonAssimilator.cxx
    WrfAssimilator.cxx
    melissa_messages.c
    server.cxx
    $<TARGET_OBJECTS:melissa_common>
)

if(INSTALL_FTI)
    add_dependencies(melissa_da_server FTI)
endif(INSTALL_FTI)

if(WITH_FTI OR WITH_FTI_THREADS)
    target_include_directories(melissa_da_server PUBLIC "${FTI_INCLUDE_DIR}")
    target_link_libraries(melissa_da_server ${FTI_LIBRARY})
    target_compile_definitions(melissa_da_server PUBLIC WITH_FTI)
  if(WITH_FTI_THREADS)
    target_compile_definitions(melissa_da_server PUBLIC WITH_FTI_THREADS)
  endif(WITH_FTI_THREADS)
endif(WITH_FTI OR WITH_FTI_THREADS)

if(INSTALL_ZMQ)
  add_dependencies(melissa_da_server ZeroMQ)
endif(INSTALL_ZMQ)

target_include_directories(melissa_da_server PUBLIC ${CMAKE_SOURCE_DIR}/pdaf-wrapper)

# Python
target_include_directories(melissa_da_server PRIVATE ${PYTHON_INCLUDE_DIR})

# get numpy include dir:
target_include_directories(melissa_da_server PRIVATE ${NUMPY_INCLUDE_DIRS})

target_link_libraries(melissa_da_server ${PYTHON_LIBRARY})

# PDAF:
target_link_libraries(melissa_da_server gfortran ${Fortran_MATH_LIBRARIES} m)

# To link against pdaf stuff:
target_link_libraries(melissa_da_server ${MPI_Fortran_LIBRARIES})
# PDAF:

target_link_libraries(melissa_da_server pdaf)
target_link_libraries(melissa_da_server gfortran m)

# To link against pdaf stuff:
target_link_libraries(melissa_da_server ${MPI_Fortran_LIBRARIES} ${Fortran_MATH_LIBRARIES})


target_link_libraries(melissa_da_server pdaf_wrapper)


INSTALL(TARGETS melissa_da_server RUNTIME DESTINATION bin)
