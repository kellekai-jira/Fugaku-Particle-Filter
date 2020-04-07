file(GLOB
    ALL_LAUNCHER
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     *.py
     )

file(GLOB
    ALL_CLUSTER
     RELATIVE
     ${CMAKE_CURRENT_SOURCE_DIR}
     cluster/*.py
     )

 install(FILES ${ALL_LAUNCHER}
     DESTINATION share/melissa-da/python)

 install(FILES ${ALL_CLUSTER}
     DESTINATION share/melissa-da/python/cluster)
