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

add_library(kvcache_net STATIC
    src/kv/client.cpp
    src/kv/net.cpp
)

target_include_directories(kvcache_net PUBLIC include)
target_compile_options(kvcache_net PRIVATE -Wall -Wextra)

add_library(kvcache_server STATIC
    src/kv/server.cpp
)

target_include_directories(kvcache_server PUBLIC include)
target_link_libraries(kvcache_server PRIVATE kvcache_core kvcache_net)
target_compile_options(kvcache_server PRIVATE -Wall -Wextra)

add_executable(KVCache-LocalCli example/local_cli_main.cpp)
target_link_libraries(KVCache-LocalCli PRIVATE kvcache_core)
target_compile_options(KVCache-LocalCli PRIVATE -Wall -Wextra)

add_executable(KVCache-RemoteCli example/remote_cli_main.cpp)
target_link_libraries(KVCache-RemoteCli PRIVATE kvcache_core kvcache_net)
target_compile_options(KVCache-RemoteCli PRIVATE -Wall -Wextra)

add_executable(KVCache-Server example/server_main.cpp)
target_link_libraries(KVCache-Server PRIVATE kvcache_server)
target_compile_options(KVCache-Server PRIVATE -Wall -Wextra)

add_executable(KVCache-Client example/client_main.cpp)
target_link_libraries(KVCache-Client PRIVATE kvcache_net)
target_compile_options(KVCache-Client PRIVATE -Wall -Wextra)

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
    tests/test_client.cpp
    tests/test_config.cpp
)
find_package(Threads REQUIRED)
target_link_libraries(kv_tests PRIVATE kvcache_core kvcache_net Catch2::Catch2WithMain Threads::Threads)
target_compile_options(kv_tests PRIVATE -Wall -Wextra)

add_test(NAME kv_tests COMMAND kv_tests)
