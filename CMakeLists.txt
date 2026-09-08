cmake_minimum_required(VERSION 3.15)

project(KVCache LANGUAGES CXX VERSION 0.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(KVCache
    src/kv/cli.cpp
    src/kv/engine.cpp
    src/kv/run.cpp
)

target_include_directories(KVCache PRIVATE include)

target_compile_options(KVCache PRIVATE -Wall -Wextra)
