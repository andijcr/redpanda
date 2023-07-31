project(boosttest_discovery_example)
cmake_minimum_required(VERSION 3.20)

# https://gitlab.kitware.com/cmake/cmake/-/merge_requests/4145
# boosttest_discover_tests cmake function, runtime discovery of test cases
include(FetchContent)
FetchContent_Declare(
  bt_discovery_module
  GIT_REPOSITORY https://github.com/Bagira80/cmake-modules
)
FetchContent_MakeAvailable(bt_discovery_module)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}/_deps/bt_discovery_module-src/modules")

find_package(Boost 1.70 CONFIG COMPONENTS unit_test_framework)

enable_testing()
include(BoostTestDiscoverTests)

add_executable(test_rpunit test_rpunit.cpp)
target_link_libraries(test_rpunit Boost::unit_test_framework)
boosttest_discover_tests(test_rpunit SKIP_DISABLED_TEST TEST_PREFIX test_rpunit.)

add_executable(test_rpfixture test_rpfixture.cpp)
target_link_libraries(test_rpfixture Boost::unit_test_framework)
boosttest_discover_tests(test_rpfixture SKIP_DISABLED_TEST TEST_PREFIX test_rpfixture.)

# usage:
# ctest --build-and-test . build --build-generator Ninja --test-command ctest
#
# ctest -R ".*_rpunit.*" also works
