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

# Add googletest directly to our build. This defines
# the gtest and gtest_main targets.
add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
        ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
        EXCLUDE_FROM_ALL)


add_subdirectory(util)
add_subdirectory(parse)
add_subdirectory(rt)
add_subdirectory(build)


add_library(libbml STATIC bind.cc)
target_link_libraries(libbml PRIVATE util)

target_include_directories(libbml
        PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        )


target_compile_features(libbml
        PRIVATE
        cxx_std_17
        )




function(add_testing_unit NAME)
    add_executable( ${NAME} ${NAME}.cc)
    target_link_libraries(${NAME} gtest gtest_main libbml)
endfunction(add_testing_unit)


#add_testing_unit(parse_test)

add_executable(test_parse test_parse.cc)
target_link_libraries(test_parse libbml)