
add_library(libbvm STATIC bvm.cc parse_asm.cc)

target_link_libraries(libbvm
        PRIVATE
        pthread
        )

add_library(libs::libbvm ALIAS libbvm)

target_include_directories(libbvm
        PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        )


target_compile_features(libbvm
        PRIVATE
        cxx_std_11
        )