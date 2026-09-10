cmake_minimum_required(VERSION 3.16)

project(EdgeScope VERSION 1.0.0 LANGUAGES CXX)

option(EDGESCOPE_BUILD_TESTS "Build EdgeScope tests" OFF)
option(EDGESCOPE_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(EDGESCOPE_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    set(EDGESCOPE_SANITIZERS "")
    if(EDGESCOPE_ENABLE_ASAN)
        list(APPEND EDGESCOPE_SANITIZERS address)
    endif()
    if(EDGESCOPE_ENABLE_UBSAN)
        list(APPEND EDGESCOPE_SANITIZERS undefined)
    endif()
    if(EDGESCOPE_SANITIZERS)
        list(JOIN EDGESCOPE_SANITIZERS "," EDGESCOPE_SANITIZER_FLAGS)
        add_compile_options(-fsanitize=${EDGESCOPE_SANITIZER_FLAGS}
                            -fno-omit-frame-pointer)
        add_link_options(-fsanitize=${EDGESCOPE_SANITIZER_FLAGS})
    endif()
endif()

add_subdirectory(common)
add_subdirectory(proto)

find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.14.1
        GIT_SHALLOW TRUE
    )
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(spdlog)
endif()

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
