if(ARDUBOT_BUILD_SIM)
    enable_testing()
    
    add_subdirectory(unity)
    add_subdirectory(unit)
    add_subdirectory(integration)
endif()