
add_executable(simulation-lorenz lorenz.cxx)
target_link_libraries(simulation-lorenz melissa_da_api)
# TODO: do we need to link against python too if using python interface?
INSTALL(TARGETS simulation-lorenz RUNTIME DESTINATION bin)

add_executable(simulation-lorenz-observations lorenz_obs.cxx)
# TODO: do we need to link against python too if using python interface?
INSTALL(TARGETS simulation-lorenz-observations RUNTIME DESTINATION bin)

