cmake_minimum_required(VERSION 3.15)

project(KVCache LANGUAGES CXX VERSION 0.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(KVCache
    main.cpp
)

target_include_directories(KVCache PRIVATE
    include
    include/kv
    src
)

