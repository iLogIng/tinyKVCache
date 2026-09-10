#ifndef KV_CONFIG_HPP
#define KV_CONFIG_HPP

#include "kv/journal.hpp"
#include "kv/net.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace kv {

struct ServerConfig {
    std::string bind = "127.0.0.1";     // IPv4 地址
    std::uint16_t port = kDefaultPort;
    std::size_t capacity = 64;
    std::string aof_path = "kv.aof";
    Fsync fsync = Fsync::Always;
};

struct ClientConfig {
    std::string host = "127.0.0.1";
    std::uint16_t port = kDefaultPort;
};

static inline void print_server_usage()
{
    std::cerr << "KVCache-Server"
                 " --port <n> [--bind <ip>] [--capacity <n>]"
                 " [--aof <path>] [--fsync always|os]\n";
}

// 解析结果: 0 成功, 1 出错(通过 err 返回), 2 已打印用法
static inline int parse_server_args(int argc, char** argv, ServerConfig& cfg,
                                    std::string& err)
{
    for (int i = 1; i < argc; ++i) {
        const std::string opt = argv[i];
        if (opt == "--help" || opt == "-h") {
            print_server_usage();
            return 2;
        }
        const bool has_next = (i + 1 < argc);
        const std::string val = has_next ? argv[i + 1] : std::string();
        if (opt == "--bind") {
            if (!has_next) {
                err = "missing value for --bind";
                return 1;
            }
            cfg.bind = val;
            ++i;
        }
        else if (opt == "--port") {
            if (!has_next) {
                err = "missing value for --port";
                return 1;
            }
            cfg.port = static_cast<std::uint16_t>(std::strtoul(val.c_str(), nullptr, 10));
            ++i;
        }
        else if (opt == "--capacity") {
            if (!has_next) {
                err = "missing value for --capacity";
                return 1;
            }
            cfg.capacity = static_cast<std::size_t>(std::strtoull(val.c_str(), nullptr, 10));
            if (cfg.capacity == 0) {
                err = "capacity must be > 0";
                return 1;
            }
            ++i;
        }
        else if (opt == "--aof") {
            if (!has_next) {
                err = "missing value for --aof";
                return 1;
            }
            cfg.aof_path = val;
            ++i;
        }
        else if (opt == "--fsync") {
            if (!has_next) {
                err = "missing value for --fsync";
                return 1;
            }
            if (val == "always") {
                cfg.fsync = Fsync::Always;
            }
            else if (val == "os") {
                cfg.fsync = Fsync::Os;
            }
            else {
                err = "fsync must be always|os";
                return 1;
            }
            ++i;
        }
        else {
            err = "unknown option: " + opt;
            return 1;
        }
    }
    return 0;
}

static inline void print_client_usage()
{
    std::cerr << "KVCache-Client [--host <ip>] [--port <n>] <command> [args...]\n";
}

// 解析结果: 0 成功, 1 出错, 2 已打印用法;
// first_pos 为首个非选项参数下标
static inline int parse_client_args(int argc, char** argv, ClientConfig& cfg,
                                    int& first_pos, std::string& err)
{
    int i = 1;
    for (; i < argc; ++i) {
        const std::string opt = argv[i];
        if (opt.rfind("--", 0) != 0 && opt != "-h") {
            break;
        }
        if (opt == "--help" || opt == "-h") {
            print_client_usage();
            return 2;
        }
        const bool has_next = (i + 1 < argc);
        const std::string val = has_next ? argv[i + 1] : std::string();
        if (opt == "--host") {
            if (!has_next) {
                err = "missing value for --host";
                return 1;
            }
            cfg.host = val;
            ++i;
        }
        else if (opt == "--port") {
            if (!has_next) {
                err = "missing value for --port";
                return 1;
            }
            cfg.port = static_cast<std::uint16_t>(std::strtoul(val.c_str(), nullptr, 10));
            ++i;
        }
        else {
            err = "unknown option: " + opt;
            return 1;
        }
    }
    first_pos = i;
    return 0;
}

}  // namespace kv

#endif  // KV_CONFIG_HPP
