add_library(build STATIC build.h build.cc)
target_include_directories(build
        PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        )

target_link_libraries(build PRIVATE util parse ir)
target_compile_features(build PRIVATE cxx_std_17)


add_executable(build_test build_test.cc)
target_link_libraries(build_test gtest gtest_main util parse build ir)