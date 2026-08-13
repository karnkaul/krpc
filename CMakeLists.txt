cmake_minimum_required(VERSION 3.24)

set(package_name krpc)
project(${package_name} VERSION "0.1.2")

option(KRPC_BUILD_EXAMPLE "Build krpc example" ${PROJECT_IS_TOP_LEVEL})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_DEBUG_POSTFIX "-d")

add_subdirectory(lib)

if(KRPC_BUILD_EXAMPLE)
  add_subdirectory(example)
endif()

if(${PROJECT_IS_TOP_LEVEL})
  enable_testing()
  add_test(NAME example COMMAND krpc-example)
endif()
