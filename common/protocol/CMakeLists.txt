find_package(Protobuf REQUIRED)

include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS p2p.proto)

add_library(melissa_protocol STATIC ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(melissa_protocol PUBLIC ${Protobuf_LIBRARY})


target_compile_options(melissa_protocol BEFORE PUBLIC -fPIC)

# CMake does not recognize .cc files as cxx automatically...
set_target_properties(melissa_protocol PROPERTIES LINKER_LANGUAGE CXX)



# Install python library
protobuf_generate_python(PROTO_PY p2p.proto)
message("python protobuf: ${PROTO_PY}")

add_custom_target(melissa_protocol_python ALL
    DEPENDS ${PROTO_PY})

install(FILES ${PROTO_PY}
 DESTINATION share/melissa-da/python)
