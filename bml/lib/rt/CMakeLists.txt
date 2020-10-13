add_library(rt STATIC rt.cc)

target_include_directories(util
        PUBLIC
        $<INSTALL_INTERFACE:util>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        )

target_link_libraries(rt PRIVATE util)
target_compile_features(rt PRIVATE cxx_std_17)

add_executable(rt_test rt_test.cc)
target_link_libraries(rt_test gtest gtest_main rt)