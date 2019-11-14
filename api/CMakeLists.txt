file(GLOB
     ALL_API
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     *.cxx
     )

add_library(melissa_api SHARED ${ALL_API} $<TARGET_OBJECTS:melissa_common>)

if(INSTALL_ZMQ)
   add_dependencies(melissa_api ZeroMQ)
endif(INSTALL_ZMQ)

target_compile_options(melissa_api BEFORE PUBLIC -fPIC)
install(TARGETS melissa_api LIBRARY DESTINATION lib)

install(FILES melissa_api.h DESTINATION include)
install(FILES melissa_api.f90 DESTINATION include)
