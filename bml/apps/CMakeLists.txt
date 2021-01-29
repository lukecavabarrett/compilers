add_executable(mlc mlc.cc)
target_link_libraries(mlc libbml)

add_executable(scratch typecheck_scratch.cpp)
target_link_libraries(scratch util)