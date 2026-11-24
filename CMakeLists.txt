cmake_minimum_required(VERSION 3.10)
project(fim CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(Threads REQUIRED)

add_library(fimcore src/sha256.cpp src/fs_utils.cpp)
target_include_directories(fimcore PUBLIC src)
target_link_libraries(fimcore PUBLIC Threads::Threads)

add_executable(fim src/main.cpp)
target_link_libraries(fim PRIVATE fimcore)

enable_testing()
add_executable(test_sha256 tests/test_sha256.cpp)
target_link_libraries(test_sha256 PRIVATE fimcore)
add_test(NAME sha256 COMMAND test_sha256)
