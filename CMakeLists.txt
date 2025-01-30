cmake_minimum_required(VERSION 3.10)
project(concurrency LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(lib)

add_executable(hello hello.cpp)
add_subdirectory(test)