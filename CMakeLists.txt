cmake_minimum_required(VERSION 3.16)

project(EdgeScope VERSION 0.6.0 LANGUAGES CXX)

option(EDGESCOPE_BUILD_TESTS "Build EdgeScope tests" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(common)
add_subdirectory(proto)
add_subdirectory(agent)
add_subdirectory(client)

if(EDGESCOPE_BUILD_TESTS)
    include(CTest)
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.15.2
            GIT_SHALLOW TRUE
        )
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()
    add_subdirectory(tests)
endif()
