add_library(util STATIC util.cc sexp.cc message.cc texp.cc)

target_include_directories(util
        PUBLIC
        $<INSTALL_INTERFACE:util>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        )

target_link_libraries(util gtest)
target_compile_features(util PRIVATE cxx_std_17)

add_executable(sexp_test sexp_test.cc)
target_link_libraries(sexp_test gtest gtest_main libbml)


add_executable(texp_test texp_test.cc)
target_link_libraries(texp_test gtest gtest_main libbml)