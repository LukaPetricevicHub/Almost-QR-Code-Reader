include(FetchContent)

find_package(Iconv REQUIRED)

option(QRCODE_BUILD_FIXTURE_GENERATOR
       "Build the developer tool that regenerates custom QR fixtures" OFF)

if(QRCODE_BUILD_FIXTURE_GENERATOR)
  set(ZXING_WRITERS OLD CACHE STRING "ZXing writer support" FORCE)
else()
  set(ZXING_WRITERS OFF CACHE STRING "ZXing writer support" FORCE)
endif()
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
  Codewords.cpp
  DataReader.cpp
  Decoder.cpp
  FormatInformation.cpp
  GaloisField256.cpp
  MessageFormatter.cpp
  Masks.cpp
  QrVersion.cpp
  ReedSolomon.cpp
  Segments.cpp
  TextEncoding.cpp)
target_compile_features(qrcode_core PUBLIC "cxx_std_${PROJECT_CXX_STANDARD}")
target_link_libraries(qrcode_core PUBLIC ZXing::ZXing Iconv::Iconv)

add_executable(qrcode main.cpp)
target_link_libraries(qrcode PRIVATE qrcode_core stb::stb)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(qrcode_core PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(qrcode PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(QRCODE_BUILD_FIXTURE_GENERATOR)
  add_executable(qrcode_generate_fixtures generate_fixtures.cpp)
  target_compile_features(qrcode_generate_fixtures
                          PRIVATE "cxx_std_${PROJECT_CXX_STANDARD}")
  target_link_libraries(qrcode_generate_fixtures PRIVATE ZXing::ZXing stb::stb)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(qrcode_generate_fixtures
                           PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

if(BUILD_TESTING)
  add_executable(qrcode_tests tests.cpp)
  target_compile_definitions(
    qrcode_tests PRIVATE QRCODE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(qrcode_tests PRIVATE qrcode_core stb::stb)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(qrcode_tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME qrcode_unit_tests COMMAND qrcode_tests)
  add_test(
    NAME qrcode_regression_tests
    COMMAND
      ${CMAKE_COMMAND} -E env QRCODE_BINARY=$<TARGET_FILE:qrcode>
      ${CMAKE_CURRENT_SOURCE_DIR}/test_qrcode.sh)
endif()
