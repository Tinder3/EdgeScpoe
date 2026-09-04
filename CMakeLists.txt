cmake_minimum_required(VERSION 3.16)

project(EdgeScope VERSION 0.1.0 LANGUAGES CXX)

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
