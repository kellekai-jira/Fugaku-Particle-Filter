file(GLOB
     ALL_API
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     *.cxx
     )

file(GLOB
     ALL_P2P
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     p2p/*.cxx
     )

file(GLOB
     ALL_STORAGE_CONTROLLER
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     p2p/StorageController/*.cxx
     )

 add_library(melissa_da_api SHARED
     ${ALL_API}
     ${ALL_P2P}
     ${ALL_STORAGE_CONTROLLER}
    $<TARGET_OBJECTS:melissa_common>
)

target_link_libraries(melissa_da_api PUBLIC melissa_protocol)

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


if(WITH_FTI OR WITH_FTI_THREADS)
    target_include_directories(melissa_da_api PUBLIC "${FTI_INCLUDE_DIR}")
    target_link_libraries(melissa_da_api PUBLIC ${FTI_LIBRARY})
    target_compile_definitions(melissa_da_api PUBLIC WITH_FTI)
  if(WITH_FTI_THREADS)
    target_compile_definitions(melissa_da_api PUBLIC WITH_FTI_THREADS)
  endif(WITH_FTI_THREADS)
endif(WITH_FTI OR WITH_FTI_THREADS)


# Python
target_include_directories(melissa_da_api PRIVATE ${PYTHON_INCLUDE_DIR})

# get numpy include dir:
target_include_directories(melissa_da_api PRIVATE ${NUMPY_INCLUDE_DIRS})

target_link_libraries(melissa_da_api PUBLIC ${PYTHON_LIBRARY})

if(INSTALL_FTI)
    add_dependencies(melissa_da_api FTI)
endif(INSTALL_FTI)

if(INSTALL_BOOST)
    add_dependencies(melissa_da_api BOOST)
endif(INSTALL_BOOST)

target_include_directories(melissa_da_api PUBLIC ${Boost_INCLUDE_DIRS})

install(TARGETS melissa_da_api LIBRARY DESTINATION lib)

install(FILES melissa_da_api.h DESTINATION include)
install(FILES ${CMAKE_BINARY_DIR}/include/melissa_da_api.f90 DESTINATION include)


install(FILES melissa_da_api.py DESTINATION share/melissa-da/python)
