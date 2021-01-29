add_library(types STATIC types.cc)
target_include_directories(types
        PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        )

target_link_libraries(types PRIVATE util ir)
target_compile_features(types PRIVATE cxx_std_17)

add_executable(type_test type_test.cc)
target_link_libraries(type_test gtest gtest_main gmock util types parse)