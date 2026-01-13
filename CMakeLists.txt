cmake_minimum_required(VERSION 3.24)

set(package_name krpc)
project(${package_name} VERSION "0.1.0")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_DEBUG_POSTFIX "-d")

add_subdirectory(lib)

add_subdirectory(test)
