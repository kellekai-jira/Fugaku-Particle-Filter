file(GLOB
     ALL_API
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     *.cxx
     )

add_library(melissa_da_api SHARED ${ALL_API}
    "../../server-p2p/messages/cpp/control_messages.pb.cc"
    $<TARGET_OBJECTS:melissa_common>)

# TODO add lib dependencie -lprotobuf

if(INSTALL_ZMQ)
   add_dependencies(melissa_da_api ZeroMQ)
endif(INSTALL_ZMQ)

add_custom_command(TARGET melissa_da_api
    PRE_BUILD
    COMMAND gcc -E -cpp -x c -P
        ${CMAKE_CURRENT_SOURCE_DIR}/melissa_da_api.i.f90
        -o ${CMAKE_BINARY_DIR}/include/melissa_da_api.f90
    COMMAND sed ${CMAKE_BINARY_DIR}/include/melissa_da_api.f90
    -e "s/__NL__/\\n/g"
    -e "s/__s__/\'/g" -i
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/melissa_da_api.i.f90
    COMMENT "Apply C Preprocessor on fortran code"
    VERBATIM
)


target_compile_options(melissa_da_api BEFORE PUBLIC -fPIC)
install(TARGETS melissa_da_api LIBRARY DESTINATION lib)
install(TARGETS melissa_da_api LIBRARY DESTINATION lib)

install(FILES melissa_da_api.h DESTINATION include)
install(FILES ${CMAKE_BINARY_DIR}/include/melissa_da_api.f90 DESTINATION include)


install(FILES melissa_da_api.py DESTINATION share/melissa-da/python)
