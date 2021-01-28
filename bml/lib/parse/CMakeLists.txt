add_library(parse STATIC parse.cc)
target_include_directories(parse
        PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        )

target_link_libraries(parse PRIVATE util ast)
target_compile_features(parse PRIVATE cxx_std_17)


add_executable(parse_test parse_test.cc)
target_link_libraries(parse_test gtest gtest_main util ast parse ir)

add_executable(bind_test bind_test.cc)
target_link_libraries(bind_test gtest gtest_main util parse ir)