cmake_minimum_required(VERSION 3.24)

project(moonlink_native_bridge LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

add_library(moonlink_native_bridge STATIC src/bridge.c)
target_include_directories(moonlink_native_bridge PUBLIC include)

add_executable(moonlink_native_bridge_tests tests/bridge_tests.c)
target_link_libraries(moonlink_native_bridge_tests PRIVATE moonlink_native_bridge)
add_test(NAME moonlink_native_bridge_tests COMMAND moonlink_native_bridge_tests)
