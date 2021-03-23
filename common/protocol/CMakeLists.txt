find_package(Protobuf REQUIRED)
include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS p2p.proto)
add_library(melissa_protocol OBJECT ${PROTO_HDRS} ${PROTO_SRC})

target_compile_options(melissa_common BEFORE PUBLIC -fPIC)

# CMake does not recognize .cc files as cxx automatically...
set_target_properties(melissa_protocol PROPERTIES LINKER_LANGUAGE CXX)

# Install python library
protobuf_generate_python(PROTO_PY p2p.proto)

install(FILES ${PROTO_PY}
 DESTINATION share/melissa-da/python)
