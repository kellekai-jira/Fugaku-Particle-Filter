
add_executable(simulation-lorenz lorenz.cxx)
#target_compile_options(simulation-lorenz PRIVATE -Nclang -fsanitize=address -fsanitize-recover=address)
#target_link_options(simulation-lorenz PRIVATE -Nclang -fsanitize=address -fsanitize-recover=address)
target_link_libraries(simulation-lorenz melissa_da_api)
# TODO: do we need to link against python too if using python interface?
INSTALL(TARGETS simulation-lorenz RUNTIME DESTINATION bin)

add_executable(simulation-lorenz-observations lorenz_obs.cxx)
# TODO: do we need to link against python too if using python interface?
INSTALL(TARGETS simulation-lorenz-observations RUNTIME DESTINATION bin)

