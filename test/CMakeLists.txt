project(${package_name}-test)

add_executable(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE
  ${package_name}::${package_name}
  $<$<BOOL:${MINGW}>:stdc++exp>
)

target_sources(${PROJECT_NAME} PRIVATE
  src/main.cpp
)
