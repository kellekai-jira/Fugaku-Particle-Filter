
# TODO: use as subproject that does find_package on melissa...
add_executable(simulation1 simulation.cxx)
target_link_libraries(simulation1 melissa_api)
INSTALL(TARGETS simulation1 RUNTIME DESTINATION bin)

# add a stateful simulation for testing
add_executable(simulation1-stateful simulation.cxx)
target_compile_definitions(simulation1-stateful PRIVATE -DBE_STATEFUL)
target_link_libraries(simulation1-stateful melissa_api)
INSTALL(TARGETS simulation1-stateful RUNTIME DESTINATION bin)
