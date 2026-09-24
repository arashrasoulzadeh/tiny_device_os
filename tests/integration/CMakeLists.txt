file(GLOB TEST_SRCS "test_*.c")

foreach(test_src ${TEST_SRCS})
    get_filename_component(test_name ${test_src} NAME_WE)
    
    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} unity kernel hal sim sim_args)
    
    if(SDL2_LIBRARIES)
        target_link_libraries(${test_name} ${SDL2_LIBRARIES})
    endif()
    
    if(PORTAUDIO_LIBRARIES)
        target_link_libraries(${test_name} ${PORTAUDIO_LIBRARIES})
    endif()
    
    target_include_directories(${test_name} PRIVATE 
        ${CMAKE_SOURCE_DIR}/tests/unity/src
        ${CMAKE_SOURCE_DIR}/kernel
        ${CMAKE_SOURCE_DIR}/hal/include
    )
    
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()