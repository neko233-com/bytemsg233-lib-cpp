cmake_minimum_required(VERSION 3.14)
project(bytemsg233 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(bytemsg233 INTERFACE)
target_include_directories(bytemsg233 INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

enable_testing()
add_executable(bytemsg233_test test/bytemsg233_test.cpp)
target_link_libraries(bytemsg233_test PRIVATE bytemsg233)
add_test(NAME bytemsg233_test COMMAND bytemsg233_test)
