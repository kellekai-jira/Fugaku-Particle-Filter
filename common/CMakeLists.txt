file(GLOB
     ALL_COMMON
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     *.c
     *.cxx
     )

add_library(melissa_common OBJECT ${ALL_COMMON})

if(INSTALL_ZMQ)
  add_dependencies(melissa_common ZeroMQ)
endif(INSTALL_ZMQ)

target_compile_options(melissa_common BEFORE PUBLIC -fPIC)
