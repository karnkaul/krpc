add_library(${package_name})
add_library(${package_name}::${package_name} ALIAS ${package_name})

target_compile_features(${package_name} PUBLIC cxx_std_23)

target_link_libraries(${PROJECT_NAME} PRIVATE
  $<$<BOOL:${WIN32}>:ws2_32.lib>
)

target_include_directories(${package_name} PRIVATE
  src
)

set(version_header include/${package_name}/build_version.hpp)
configure_file(src/build_version.hpp.in ${version_header} @ONLY)

file(GLOB_RECURSE headers LIST_DIRECTORIES false "include/*.hpp")

target_sources(${package_name} PUBLIC FILE_SET HEADERS
  BASE_DIRS
  include
  "${CMAKE_CURRENT_BINARY_DIR}/include"

  FILES
  ${headers}
  "${CMAKE_CURRENT_BINARY_DIR}/${version_header}"
)

target_sources(${package_name} PRIVATE
  src/krpc.cpp
)
