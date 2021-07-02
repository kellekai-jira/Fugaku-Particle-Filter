add_library(melissa_common OBJECT
    Part.cxx
    ZeroMQ.cxx
    melissa_utils.c
    utils.cxx
    melissa_messages.c
)


if(INSTALL_ZMQ)
  add_dependencies(melissa_common ZeroMQ)
endif(INSTALL_ZMQ)

target_compile_options(melissa_common BEFORE PUBLIC -fPIC)

install(FILES melissa_da_stype.h DESTINATION include)


# comm4py

add_library(melissa_comm4py SHARED comm4py.c)
target_link_libraries(melissa_comm4py PUBLIC zmq)
install(
    TARGETS melissa_comm4py
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
