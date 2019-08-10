cmake_minimum_required(VERSION 2.8)

project(proc_maps)

set(COMPILER_WARNINGS -Wpedantic -Wall -Wextra)

add_library(${PROJECT_NAME} STATIC proc_maps.c)

target_include_directories(
    ${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(${PROJECT_NAME} PRIVATE ${COMPILER_WARNINGS})

if(BUILD_PROC_MAPS_EXAMPLE)

    add_executable(proc_maps_example example.c)

    target_include_directories(
        proc_maps_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    target_compile_options(proc_maps_example PRIVATE ${COMPILER_WARNINGS})

    target_link_libraries(proc_maps_example PRIVATE ${PROJECT_NAME})

endif()
