file(COPY_FILE
     "${PROJECT_SOURCE_DIR}/resources/vigenere/wordlist-en-US/en_US.txt"
     "${CMAKE_CURRENT_BINARY_DIR}/en_US.txt")

add_executable(vigenere main.cpp)
target_compile_features(vigenere PRIVATE "cxx_std_${PROJECT_CXX_STANDARD}")
