add_library(ir STATIC ir.cc lang.cc)
target_include_directories(ir
        PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        )

target_link_libraries(ir PRIVATE util)
target_compile_features(ir PRIVATE cxx_std_17)


add_executable(ir_test ir_test.cc)
target_link_libraries(ir_test gtest gtest_main gmock util ir)
