cmake_minimum_required(VERSION 3.24)

project(moonlink_native_bridge LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

option(MOONLINK_WITH_LIBDATACHANNEL "Build the fixed libdatachannel loopback probe" OFF)
set(MOONLINK_LIBDATACHANNEL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/paullouisageneau-libdatachannel-d9617a5" CACHE PATH "Fixed libdatachannel source directory")
set(MOONLINK_OPENSSL_ROOT "" CACHE PATH "OpenSSL root used only for the libdatachannel build")

add_library(moonlink_native_bridge STATIC src/bridge.c)
target_include_directories(moonlink_native_bridge PUBLIC include)
target_link_libraries(
  moonlink_native_bridge
  PRIVATE
    user32
    d3d11
    dxgi
    dxguid
    mfplat
    mf
    mfuuid
    ole32
)

add_executable(moonlink_native_bridge_tests tests/bridge_tests.c)
target_link_libraries(moonlink_native_bridge_tests PRIVATE moonlink_native_bridge)
add_test(NAME moonlink_native_bridge_tests COMMAND moonlink_native_bridge_tests)

if(MOONLINK_WITH_LIBDATACHANNEL)
  if(NOT EXISTS "${MOONLINK_LIBDATACHANNEL_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Fixed libdatachannel source is absent; follow native/LIBDATACHANNEL_LOCK.md first.")
  endif()
  if(NOT EXISTS "${MOONLINK_OPENSSL_ROOT}/include/openssl/ssl.h")
    message(FATAL_ERROR "MOONLINK_OPENSSL_ROOT must reference an OpenSSL development root.")
  endif()

  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
  set(NO_TESTS ON CACHE BOOL "" FORCE)
  set(LIBSRTP_TEST_APPS OFF CACHE BOOL "" FORCE)
  set(OPENSSL_ROOT_DIR "${MOONLINK_OPENSSL_ROOT}" CACHE PATH "" FORCE)
  add_subdirectory("${MOONLINK_LIBDATACHANNEL_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/libdatachannel")

  add_executable(moonlink_native_transport_loopback_tests tests/transport_loopback.c)
  target_include_directories(moonlink_native_transport_loopback_tests PRIVATE "${MOONLINK_LIBDATACHANNEL_SOURCE_DIR}/include")
  target_link_libraries(moonlink_native_transport_loopback_tests PRIVATE datachannel-static)
  set_target_properties(moonlink_native_transport_loopback_tests PROPERTIES LINKER_LANGUAGE CXX)
  add_test(NAME moonlink_native_transport_loopback_tests COMMAND moonlink_native_transport_loopback_tests)
endif()
