add_test(NAME simulation5-python
    COMMAND ${in_env} python3 ./script.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
