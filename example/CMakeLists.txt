project(krpc-example)

add_executable(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE
  krpc::krpc
  $<$<BOOL:${MINGW}>:stdc++exp>
)

target_sources(${PROJECT_NAME} PRIVATE
  src/main.cpp
)
