if (WITH_FTI)
    install(FILES config.fti
        DESTINATION ${DATADIR_INSTALL})
    install(FILES config-p2p-runner.fti
        DESTINATION ${DATADIR_INSTALL})
endif()
