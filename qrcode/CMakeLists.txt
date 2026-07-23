include(FetchContent)

set(ZXING_WRITERS OFF CACHE STRING "ZXing writer support" FORCE)
set(ZXING_C_API OFF CACHE BOOL "ZXing C API" FORCE)
set(ZXING_EXAMPLES OFF CACHE BOOL "ZXing examples" FORCE)

FetchContent_Declare(
  ZXing
  GIT_REPOSITORY https://github.com/zxing-cpp/zxing-cpp.git
  GIT_TAG v3.0.2)

FetchContent_MakeAvailable(ZXing)

zxing_add_package_stb()

add_library(
  qrcode_core
  STATIC
  Bitstream.cpp
  DataReader.cpp
  Decoder.cpp
  Masks.cpp
  QrVersion.cpp
  Segments.cpp)
target_compile_features(qrcode_core PUBLIC "cxx_std_${PROJECT_CXX_STANDARD}")
target_link_libraries(qrcode_core PUBLIC ZXing::ZXing)

add_executable(qrcode main.cpp)
target_link_libraries(qrcode PRIVATE qrcode_core stb::stb)

if(BUILD_TESTING)
  add_executable(qrcode_tests tests.cpp)
  target_link_libraries(qrcode_tests PRIVATE qrcode_core)
  add_test(NAME qrcode_unit_tests COMMAND qrcode_tests)
  add_test(
    NAME qrcode_regression_tests
    COMMAND
      ${CMAKE_COMMAND} -E env QRCODE_BINARY=$<TARGET_FILE:qrcode>
      ${CMAKE_CURRENT_SOURCE_DIR}/test_qrcode.sh)
endif()
