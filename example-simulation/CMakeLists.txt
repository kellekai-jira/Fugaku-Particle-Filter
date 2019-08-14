
add_executable(example_simulation simulation.cxx)
target_link_libraries(example_simulation melissa_api)
INSTALL(TARGETS example_simulation RUNTIME DESTINATION bin)
