cmake_minimum_required(VERSION 3.20)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(quanta)

add_executable(quanta main.cpp)

target_include_directories(quanta PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)