add_executable(sample sample.cc)
target_link_libraries(sample libbvm)
target_include_directories(sample PRIVATE /usr/include/postgres)