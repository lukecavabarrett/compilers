
add_library(libbml STATIC errmsg.cc util.cc ast.cc parse.cc bind.cc)

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

configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
    message(FATAL_ERROR "CMake step for googletest failed: ${result}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )
if(result)
    message(FATAL_ERROR "Build step for googletest failed: ${result}")
endif()

# Prevent overriding the parent project's compiler/linker
# settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
        ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
        EXCLUDE_FROM_ALL)


function(add_testing_unit NAME)
    add_executable( ${NAME} ${NAME}.cc)
    target_link_libraries(${NAME} gtest gtest_main libbml)
endfunction(add_testing_unit)


add_testing_unit(parse_test)
add_testing_unit(sexp_test)

add_executable(test_parse test_parse.cc)
target_link_libraries(test_parse libbml)