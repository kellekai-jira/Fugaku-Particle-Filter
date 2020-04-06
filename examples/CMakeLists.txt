function(python_test test_name)
    #message("Source_dir: ${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME ${test_name} COMMAND ${in_env} python3 ./tests.py ${test_name} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

add_subdirectory(simulation1)
add_subdirectory(simulation2-pdaf)
add_subdirectory(simulation3-empty)
