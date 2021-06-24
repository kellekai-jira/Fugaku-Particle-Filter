add_subdirectory(protocol)

add_library(melissa_common OBJECT
    Part.cxx
    ZeroMQ.cxx
    melissa_utils.c
    utils.cxx
    melissa_messages.c
)



target_compile_options(melissa_common BEFORE PUBLIC -fPIC)


if(INSTALL_ZMQ)
  add_dependencies(melissa_common ZeroMQ)
endif(INSTALL_ZMQ)

install(FILES melissa_da_stype.h DESTINATION include)

# comm4py

add_library(melissa_comm4py SHARED melissa_comm4py.c melissa_utils.c melissa_messages.c)
target_link_libraries(melissa_comm4py PUBLIC zmq)
install(
    TARGETS melissa_comm4py
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
