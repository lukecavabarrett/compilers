add_executable(mlc mlc.cc)
target_link_libraries(mlc libbml)

add_executable(typecheck_scratch typecheck_scratch.cpp)
target_link_libraries(typecheck_scratch util)