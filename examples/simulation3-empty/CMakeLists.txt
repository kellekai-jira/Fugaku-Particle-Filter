
add_executable(simulation3-empty simulation.cxx)
target_link_libraries(simulation3-empty melissa_api)
INSTALL(TARGETS simulation3-empty RUNTIME DESTINATION bin)

add_test(NAME simulation3-empty
    COMMAND ${in_env} python3 ./script.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
