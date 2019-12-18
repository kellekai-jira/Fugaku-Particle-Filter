
add_executable(simulation3-empty simulation.cxx)
target_link_libraries(simulation3-empty melissa_api)
INSTALL(TARGETS simulation3-empty RUNTIME DESTINATION bin)
