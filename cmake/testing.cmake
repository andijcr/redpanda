include(CMakeParseArguments)
include(BoostTestDiscoverTests)
enable_testing()
set(RUNNER "${PROJECT_SOURCE_DIR}/tools/cmake_test.py")
option(RP_ENABLE_TESTS "Useful for disabling all tests" ON)
option(RP_ENABLE_FIXTURE_TESTS "control if integrations are bulit and ran" ON)
option(RP_ENABLE_UNIT_TESTS "control if unit tests are bulit and ran" ON)
option(RP_ENABLE_BENCHMARK_TESTS "control if benchmarks are bulit and ran" ON)
if(NOT RP_ENABLE_TESTS)
  set(RP_ENABLE_FIXTURE_TESTS  OFF)
  set(RP_ENABLE_UNIT_TESTS  OFF)
  set(RP_ENABLE_BENCHMARK_TESTS  OFF)
endif()

set(FIXTURE_TESTS "")
set(UNIT_TESTS "")
set(BENCHMARK_TESTS "")

message(STATUS "RP_ENABLE_FIXTURE_TESTS=${RP_ENABLE_FIXTURE_TESTS}")
message(STATUS "RP_ENABLE_UNIT_TESTS=${RP_ENABLE_UNIT_TESTS}")
message(STATUS "RP_ENABLE_BENCHMARK_TESTS=${RP_ENABLE_BENCHMARK_TESTS}")

function (rp_test)
  set(options
    FIXTURE_TEST UNIT_TEST BENCHMARK_TEST)
  set(oneValueArgs BINARY_NAME TIMEOUT PREPARE_COMMAND POST_COMMAND)
  set(multiValueArgs
    INCLUDES
    SOURCES
    LIBRARIES
    DEFINITIONS
    INPUT_FILES
    BUILD_DEPENDENCIES
    ENV
    LABELS
    ARGS
    SKIP_BUILD_TYPES)
  cmake_parse_arguments(RP_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(RP_TEST_UNIT_TEST AND RP_ENABLE_UNIT_TESTS)
    set(RP_TEST_BINARY_NAME "${RP_TEST_BINARY_NAME}_rpunit")
    set(UNIT_TESTS "${UNIT_TESTS} ${RP_TEST_BINARY_NAME}")
  endif()
  if(RP_TEST_FIXTURE_TEST AND RP_ENABLE_FIXTURE_TESTS)
    set(RP_TEST_BINARY_NAME "${RP_TEST_BINARY_NAME}_rpfixture")
    set(FIXTURE_TESTS "${FIXTURE_TESTS} ${RP_TEST_BINARY_NAME}")
  endif()
  if(RP_TEST_BENCHMARK_TEST AND RP_ENABLE_BENCHMARK_TESTS)
    if(CMAKE_BUILD_TYPE MATCHES Debug)
      # given a benchmark test but build is not release
      return()
    endif()
    set(RP_TEST_BINARY_NAME "${RP_TEST_BINARY_NAME}_rpbench")
    set(BENCHMARK_TESTS "${BENCHMARK_TESTS} ${RP_TEST_BINARY_NAME}")
  endif()

  set(files_to_copy_list "")
  foreach(i ${RP_TEST_INPUT_FILES})
    list(APPEND files_to_copy_list "--copy_file ${i}")
  endforeach()

  string(JOIN " " files_to_copy ${files_to_copy_list})

  set(prepare_command "")
  if (RP_TEST_PREPARE_COMMAND)
      set(prepare_command "--pre='${RP_TEST_PREPARE_COMMAND}'")
  endif()

  set(post_command "")
  if (RP_TEST_POST_COMMAND)
      set(post_command "--post='${RP_TEST_POST_COMMAND}'")
  endif()

  add_executable(
    ${RP_TEST_BINARY_NAME} "${RP_TEST_SOURCES}")
  target_link_libraries(
    ${RP_TEST_BINARY_NAME} PUBLIC "${RP_TEST_LIBRARIES}")
  if(${BUILD_DEPENDENCIES})
    add_dependencies(${RP_TEST_BINARY_NAME} ${BUILD_DEPENDENCIES})
  endif()

  foreach(i ${RP_TEST_INCLUDES})
    target_include_directories(${RP_TEST_BINARY_NAME} PUBLIC ${i})
  endforeach()

  foreach(i ${RP_TEST_DEFINITIONS})
    target_compile_definitions(${RP_TEST_BINARY_NAME} PRIVATE "${i}")
  endforeach()

  install(TARGETS ${RP_TEST_BINARY_NAME} DESTINATION bin)

  # all tests are compiled for every build type
  # some tests are not run for every build type
  set(skip_test FALSE)
  foreach(type ${RP_TEST_SKIP_BUILD_TYPES})
    if(CMAKE_BUILD_TYPE STREQUAL ${type})
      set(skip_test TRUE)
    endif()
  endforeach()

  if(RP_TEST_UNIT_TEST)
  if(NOT RP_TEST_ARGS)
    # For tests that don't set some explicit args (some of them do a -c 1), set
    # an explicit core count, to avoid unit tests running differently on machines
    # with different core counts (this also speeds up some tests running on many-core
    # machines.
    set(RP_TEST_ARGS "-- -c 4")
  endif()
  endif()

  if(RP_TEST_BENCHMARK_TEST)
    if(NOT RP_TEST_ARGS)
      # For tests that don't set some explicit args (some of them do a -c 1), set
      # an explicit core count, to avoid unit tests running differently on machines
      # with different core counts (this also speeds up some tests running on many-core
      # machines.
      set(RP_TEST_ARGS "-c 1")
    endif()
  endif()

  if(NOT skip_test)
    # separate_arguments is needed to split "-- -c 1" into ["--","-c","1"] 
    if(RP_TEST_BENCHMARK_TEST)
      add_test (
        NAME ${RP_TEST_BINARY_NAME}
        COMMAND bash -c "${RUNNER} --binary=$<TARGET_FILE:${RP_TEST_BINARY_NAME}> ${prepare_command} ${post_command} ${files_to_copy} ${RP_TEST_ARGS} "
        )
      set_tests_properties(${RP_TEST_BINARY_NAME} PROPERTIES LABELS "${RP_TEST_LABELS}")
      if(RP_TEST_TIMEOUT)
        set_tests_properties(${RP_TEST_BINARY_NAME}
          PROPERTIES TIMEOUT ${RP_TEST_TIMEOUT})
      endif()
      set_property(TEST ${RP_TEST_BINARY_NAME} PROPERTY ENVIRONMENT "${RP_TEST_ENV}")
    else()
      separate_arguments(RP_TEST_ARGS UNIX_COMMAND ${RP_TEST_ARGS})
      add_custom_target("${RP_TEST_BINARY_NAME}_runner_base"
        COMMAND ${RUNNER} --binary=$<TARGET_FILE:${RP_TEST_BINARY_NAME}> 
        )
      list(APPEND test_props LABELS "${RP_TEST_LABELS}" ENVIRONMENT "${RP_TEST_ENV}")
      if(RP_TEST_TIMEOUT)
        list(APPEND test_props TIMEOUT ${RP_TEST_TIMEOUT})
      endif()
      boosttest_discover_tests(
        "${RP_TEST_BINARY_NAME}_runner_base"
        TARGET_FILE $<TARGET_FILE:${RP_TEST_BINARY_NAME}>
        EXTRA_ARGS ${prepare_command} ${post_command} ${files_to_copy} ${RP_TEST_ARGS}
        TEST_PREFIX ${RP_TEST_BINARY_NAME}.
        SKIP_DISABLED_TESTS
        PROPERTIES ${test_props}
        DISCOVERY_MODE PRE_TEST
        )
    endif()
    #add_test (
    #  NAME ${RP_TEST_BINARY_NAME}
    #  COMMAND bash -c "${RUNNER} --binary=$<TARGET_FILE:${RP_TEST_BINARY_NAME}> ${prepare_command} ${post_command} ${files_to_copy} ${RP_TEST_ARGS} "
    #  )
    #set_tests_properties(${RP_TEST_BINARY_NAME} PROPERTIES LABELS "${RP_TEST_LABELS}")
    #if(RP_TEST_TIMEOUT)
    #  set_tests_properties(${RP_TEST_BINARY_NAME}
    #    PROPERTIES TIMEOUT ${RP_TEST_TIMEOUT})
    #endif()
    #set_property(TEST ${RP_TEST_BINARY_NAME} PROPERTY ENVIRONMENT "${RP_TEST_ENV}")
  endif()
endfunction()

if(RP_ENABLE_TESTS)
  add_custom_target(check
    COMMAND ctest --output-on-failure
    DEPENDS "${UNIT_TESTS} ${FIXTURE_TESTS} ${BENCHMARK_TESTS}")
endif()
