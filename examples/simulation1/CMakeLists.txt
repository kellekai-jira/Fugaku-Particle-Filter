
# TODO: use as subproject that does find_package on melissa...
add_executable(simulation1 simulation.cxx)
target_link_libraries(simulation1 melissa_api)
INSTALL(TARGETS simulation1 RUNTIME DESTINATION bin)

# add a stateful simulation for testing
add_executable(simulation1-stateful simulation.cxx)
target_compile_definitions(simulation1-stateful PRIVATE -DBE_STATEFUL)
target_link_libraries(simulation1-stateful melissa_api)
INSTALL(TARGETS simulation1-stateful RUNTIME DESTINATION bin)

add_executable(simulation1-hidden simulation.cxx)
target_compile_definitions(simulation1-hidden PRIVATE -DUSE_HIDDEN_STATE -DBE_STATEFUL)
target_link_libraries(simulation1-hidden melissa_api)
INSTALL(TARGETS simulation1-hidden RUNTIME DESTINATION bin)



add_test(NAME "test-example-simulation1" COMMAND ${in_env} python3 ./script.py ${test_name} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

python_test("test-check-stateless")

python_test("test-different-parallelism")

python_test("test-crashing-runner")

python_test("test-crashing-server1")

