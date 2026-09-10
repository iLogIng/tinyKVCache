cmake_minimum_required(VERSION 3.15)

project(KVCache LANGUAGES CXX VERSION 0.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(kvcache_core STATIC
    src/kv/cli.cpp
    src/kv/engine.cpp
    src/kv/journal.cpp
    src/kv/regcmd.cpp
)

target_include_directories(kvcache_core PUBLIC include)
target_compile_options(kvcache_core PRIVATE -Wall -Wextra)

add_executable(KVCache src/kv/run.cpp)
target_link_libraries(KVCache PRIVATE kvcache_core)
target_compile_options(KVCache PRIVATE -Wall -Wextra)

add_library(kvcache_net STATIC
    src/kv/net.cpp
)

target_include_directories(kvcache_net PUBLIC include)
target_compile_options(kvcache_net PRIVATE -Wall -Wextra)

add_executable(KVCacheServer src/kv/server.cpp)
target_link_libraries(KVCacheServer PRIVATE kvcache_core kvcache_net)
target_compile_options(KVCacheServer PRIVATE -Wall -Wextra)

add_executable(KVCacheClient src/kv/client.cpp)
target_link_libraries(KVCacheClient PRIVATE kvcache_net)
target_compile_options(KVCacheClient PRIVATE -Wall -Wextra)

enable_testing()

include(FetchContent)
FetchContent_Declare(catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(catch2)

add_executable(kv_tests
    tests/test_engine.cpp
    tests/test_cli.cpp
    tests/test_journal.cpp
    tests/test_net.cpp
)
target_link_libraries(kv_tests PRIVATE kvcache_core kvcache_net Catch2::Catch2WithMain)
target_compile_options(kv_tests PRIVATE -Wall -Wextra)

add_test(NAME kv_tests COMMAND kv_tests)
