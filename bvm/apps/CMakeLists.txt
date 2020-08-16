add_executable(sample sample.cc)
target_link_libraries(sample libbvm)

add_executable(run run.cc)
target_link_libraries(run libbvm)
