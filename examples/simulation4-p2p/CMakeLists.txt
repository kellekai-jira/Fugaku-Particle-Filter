
add_executable(simulation4-p2p simulation.cxx)
target_link_libraries(simulation4-p2p melissa_da_api)
# TODO: do we need to link against python too if using python interface?
INSTALL(TARGETS simulation4-p2p RUNTIME DESTINATION bin)

