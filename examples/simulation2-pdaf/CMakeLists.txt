file(GLOB
  ALL_SIMULATION_2_PDAF
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     *.F90
     )

add_executable(simulation2-pdaf ${ALL_SIMULATION_2_PDAF})


# PDAF:
target_link_libraries(simulation2-pdaf gfortran ${Fortran_MATH_LIBRARIES} m)

# To link against pdaf stuff:
target_link_libraries(simulation2-pdaf ${MPI_Fortran_LIBRARIES})

target_link_libraries(simulation2-pdaf melissa_da_api)

target_include_directories(simulation2-pdaf PRIVATE ${CMAKE_BINARY_DIR}/include)


# seems to be sometimes needed on juwels?! on other systems this poses problems
# since the program won't start at main
#set_target_properties(simulation2-pdaf PROPERTIES LINK_FLAGS "-nostartfiles")

INSTALL(TARGETS simulation2-pdaf RUNTIME DESTINATION bin)
#${CMAKE_CURRENT_SOURCE_DIR}/test.sh

add_test(NAME simulation2-pdaf
    COMMAND ${in_env} python3 ./script.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

python_test("test-example-simulation2")
