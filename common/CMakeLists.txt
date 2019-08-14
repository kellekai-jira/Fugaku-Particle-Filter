file(GLOB
     ALL_COMMON
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     *.cxx
     )


add_library(melissa_common OBJECT ${ALL_COMMON})
target_compile_options(melissa_common BEFORE PUBLIC -fPIC)
