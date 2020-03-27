
# TODO: use as subproject that does find_package on melissa...
add_executable(simulation1 simulation.cxx)
target_link_libraries(simulation1 melissa_api)
INSTALL(TARGETS simulation1 RUNTIME DESTINATION bin)
add_test(NAME stateless
    COMMAND ${in_env} ${CMAKE_CURRENT_SOURCE_DIR}/../check_stateless.sh ${CMAKE_CURRENT_BINARY_DIR}/simulation1)

# add a stateful simulation for testing
add_executable(simulation1-stateful simulation.cxx)
target_compile_definitions(simulation1-stateful PRIVATE -DBE_STATEFUL)
target_link_libraries(simulation1-stateful melissa_api)
INSTALL(TARGETS simulation1-stateful RUNTIME DESTINATION bin)

# we expect a no here, so negate the result!
add_test(NAME stateful
    COMMAND ${in_env} ${CMAKE_CURRENT_SOURCE_DIR}/../check_stateless.sh ${CMAKE_CURRENT_BINARY_DIR}/simulation1-stateful)

set_tests_properties(stateful PROPERTIES WILL_FAIL TRUE)

# add a stateful simulation for testing using hidden states (so at the end it is stateless
add_executable(simulation1-hidden simulation.cxx)
target_compile_definitions(simulation1-hidden PRIVATE -DUSE_HIDDEN_STATE -DBE_STATEFUL)
target_link_libraries(simulation1-hidden melissa_api)
INSTALL(TARGETS simulation1-hidden RUNTIME DESTINATION bin)
add_test(NAME stateless-hidden
    COMMAND ${in_env} ${CMAKE_CURRENT_SOURCE_DIR}/../check_stateless.sh ${CMAKE_CURRENT_BINARY_DIR}/simulation1-hidden)

add_test(NAME fault-tolerance
    COMMAND ${in_env} ./test-fault-tolerance.sh WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME launcher COMMAND ${in_env} ./run-launcher.sh WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
