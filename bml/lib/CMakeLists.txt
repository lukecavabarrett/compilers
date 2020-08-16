
add_library(libbml STATIC util.cc ast.cc parse.cc)

target_link_libraries(libbml
        PRIVATE
        pthread
        )

add_library(libs::libbml ALIAS libbml)

target_include_directories(libbml
        PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        )


target_compile_features(libbml
        PRIVATE
        cxx_std_11
        )

add_executable(test_parse test_parse.cc)
target_link_libraries(test_parse libbml)