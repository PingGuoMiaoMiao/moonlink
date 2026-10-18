cmake_minimum_required(VERSION 3.24)

project(moonlink_native_bridge LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

option(MOONLINK_WITH_LIBDATACHANNEL "Build the fixed libdatachannel loopback probe" OFF)
set(MOONLINK_LIBDATACHANNEL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/paullouisageneau-libdatachannel-d9617a5" CACHE PATH "Fixed libdatachannel source directory")
set(MOONLINK_OPENSSL_ROOT "" CACHE PATH "OpenSSL root used only for the libdatachannel build")

add_library(moonlink_native_bridge STATIC
  src/bridge.c
  src/encoder_session.c
  src/decoder_session.c
  src/viewer_session.c
  src/handle_table.c
  src/signaling.c
  src/h264_depacketizer.c
)
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
    ws2_32
)

# Transport session requires libdatachannel; built only when available.
if(MOONLINK_WITH_LIBDATACHANNEL)
  target_sources(moonlink_native_bridge PRIVATE src/transport_session.c)
  target_include_directories(moonlink_native_bridge PRIVATE "${MOONLINK_LIBDATACHANNEL_SOURCE_DIR}/include")
  target_link_libraries(moonlink_native_bridge PRIVATE datachannel-static)
  target_compile_definitions(moonlink_native_bridge PUBLIC MOONLINK_TRANSPORT_ENABLED=1)
endif()

add_executable(moonlink_native_bridge_tests tests/bridge_tests.c)
target_link_libraries(moonlink_native_bridge_tests PRIVATE moonlink_native_bridge)
add_test(NAME moonlink_native_bridge_tests COMMAND moonlink_native_bridge_tests)

add_executable(moonlink_e2e_loopback tests/e2e_loopback.c)
target_link_libraries(moonlink_e2e_loopback PRIVATE moonlink_native_bridge)
add_test(NAME moonlink_e2e_loopback COMMAND moonlink_e2e_loopback)

add_executable(moonlink_decoder_probe tests/decoder_probe.c)
target_link_libraries(moonlink_decoder_probe PRIVATE moonlink_native_bridge)

# Round-trip diagnostic. Not registered as a CTest case yet: on hosts whose
# only H.264 decoder is the DXVA-capable Microsoft MFT it reports zero decoded
# frames until a D3D11 device manager is attached (see ROADMAP phase 2).
add_executable(moonlink_decoder_roundtrip tests/decoder_loop_tests.c)
target_link_libraries(moonlink_decoder_roundtrip PRIVATE moonlink_native_bridge)

add_executable(moonlink_depacketizer_tests tests/depacketizer_tests.c)
target_include_directories(moonlink_depacketizer_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(moonlink_depacketizer_tests PRIVATE moonlink_native_bridge)
add_test(NAME moonlink_depacketizer_tests COMMAND moonlink_depacketizer_tests)

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

  # Host and Controller applications.
  add_executable(moonlink_host apps/host_main.c)
  target_link_libraries(moonlink_host PRIVATE moonlink_native_bridge)
  set_target_properties(moonlink_host PROPERTIES LINKER_LANGUAGE CXX)

  add_executable(moonlink_controller apps/controller_main.c)
  target_link_libraries(moonlink_controller PRIVATE moonlink_native_bridge)
  set_target_properties(moonlink_controller PROPERTIES LINKER_LANGUAGE CXX)
endif()
