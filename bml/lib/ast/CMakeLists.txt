add_library(ast STATIC ast.cc)
target_include_directories(ast
        PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        )

target_link_libraries(ast PUBLIC util ir types)
target_compile_features(ast PRIVATE cxx_std_17)

