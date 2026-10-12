file(GLOB TEST_SRCS "test_*.c")

# Create an interface library that groups sim, game, and apps with
# --start-group/--end-group to resolve the sim<->apps circular dependency
add_library(sim_apps_group INTERFACE)
target_link_libraries(sim_apps_group INTERFACE "-Wl,--start-group" sim game
                                               apps "-Wl,--end-group")
target_include_directories(
  sim_apps_group INTERFACE ${CMAKE_SOURCE_DIR}/sim ${CMAKE_SOURCE_DIR}/game
                           ${CMAKE_SOURCE_DIR}/apps)

foreach(test_src ${TEST_SRCS})
  get_filename_component(test_name ${test_src} NAME_WE)

  add_executable(${test_name} ${test_src})

  # test_headless_exit and test_sim_args test functions from sim_args.c, link
  # with sim_args instead of sim to avoid duplicate main
  if(test_name STREQUAL "test_headless_exit" OR test_name STREQUAL
                                                "test_sim_args")
    target_link_libraries(${test_name} unity kernel hal sim_args)
  elseif(test_name STREQUAL "test_coverage_gate")
    target_link_libraries(${test_name} unity kernel hal sim_apps_group)
    target_compile_definitions(
      ${test_name} PRIVATE COVERAGE_SCRIPT_DIR="${CMAKE_SOURCE_DIR}")
  elseif(test_name STREQUAL "test_tdd_hook")
    target_link_libraries(${test_name} unity kernel hal sim_apps_group game)
    target_compile_definitions(${test_name}
                               PRIVATE REPO_ROOT_DIR="${CMAKE_SOURCE_DIR}")
  elseif(test_name STREQUAL "test_littlefs_vfs")
    # LittleFS integration has issues in sim mode, skip for now
    target_link_libraries(${test_name} unity kernel hal sim_apps_group)
  else()
    target_link_libraries(
      ${test_name}
      unity
      kernel
      hal
      sim_apps_group
      game
      stdapps)
  endif()

  if(SDL2_LIBRARIES)
    target_link_libraries(${test_name} ${SDL2_LIBRARIES})
  endif()
  if(SDL2_LIBRARY_DIRS)
    target_link_directories(${test_name} PUBLIC ${SDL2_LIBRARY_DIRS})
  endif()
  if(SDL2_LDFLAGS_OTHER)
    target_link_options(${test_name} PUBLIC ${SDL2_LDFLAGS_OTHER})
  endif()

  if(PORTAUDIO_LIBRARIES)
    target_link_libraries(${test_name} ${PORTAUDIO_LIBRARIES})
  endif()
  if(PORTAUDIO_LIBRARY_DIRS)
    target_link_directories(${test_name} PUBLIC ${PORTAUDIO_LIBRARY_DIRS})
  endif()

  target_include_directories(
    ${test_name}
    PRIVATE ${CMAKE_SOURCE_DIR}/tests/unity/src ${CMAKE_SOURCE_DIR}/kernel
            ${CMAKE_SOURCE_DIR}/hal/include ${CMAKE_SOURCE_DIR}/game
            ${CMAKE_SOURCE_DIR}/link ${CMAKE_SOURCE_DIR}/apps)

  add_test(NAME ${test_name} COMMAND ${test_name})

  if(test_name STREQUAL "test_littlefs_vfs")
    # LittleFS integration has issues in sim mode, skip for now
    set_tests_properties(${test_name} PROPERTIES WILL_FAIL TRUE)
  endif()
endforeach()
