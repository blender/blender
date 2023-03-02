
cmake_minimum_required(VERSION 3.15)
project(FindRust LANGUAGES CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../../../cmake" ${CMAKE_MODULE_PATH})

# make sure find_package(Rust) can be used more than once
find_package(Rust REQUIRED)
find_package(Rust REQUIRED)
