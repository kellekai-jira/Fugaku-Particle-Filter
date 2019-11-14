
add_executable(simulation1 simulation.cxx)
target_link_libraries(simulation1 melissa_api)
INSTALL(TARGETS simulation1 RUNTIME DESTINATION bin)
