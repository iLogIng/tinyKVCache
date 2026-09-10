#include "kv/cli.hpp"
#include "kv/client.hpp"
#include "kv/config.hpp"

#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

// 用户输入行 -> 请求;
// 空行返回 false, exit/quit 由调用方判断
bool to_request(const std::string& line, kv::Request& request)
{
    std::vector<std::string> tokens = kv::tokenize(line);
    if (tokens.empty()) {
        return false;
    }
    request.cmd = tokens[0];
    request.args.assign(tokens.begin() + 1, tokens.end());
    return true;
}

}  // namespace

int main(int argc, char* argv[])
{
    kv::ClientConfig config;
    std::string err;
    int first_pos = 0;
    const int rc = kv::parse_client_args(argc, argv, config, first_pos, err);
    // --help
    if (rc == 2) {
        return 0;
    }
    // error
    if (rc == 1) {
        std::cerr << "error: " << err << '\n';
        kv::print_client_usage();
        return 1;
    }

    kv::Client client;
    if (!client.connect(config.host, config.port)) {
        std::cerr << "error: connect(" << config.host << ':' << config.port << ")\n";
        return 1;
    }

    // 非 tty: 整批发送后统一收响应(流水线)
    if (!::isatty(STDIN_FILENO)) {
        std::vector<kv::Request> requests;
        std::string line;
        while (std::getline(std::cin, line)) {
            kv::Request request;
            if (!to_request(line, request)) {
                continue;
            }
            if (request.cmd == "exit" || request.cmd == "quit") {
                break;
            }
            requests.push_back(std::move(request));
        }
        if (requests.empty()) {
            return 0;
        }
        if (!client.send_batch(requests)) {
            std::cerr << "error: send failed\n";
            return 1;
        }
        std::vector<std::string> out;
        if (!client.recv_batch(requests.size(), out)) {
            std::cerr << "error: recv failed\n";
            return 1;
        }
        for (const std::string& payload : out) {
            std::cout << payload;
        }
        return 0;
    }

    // tty: 逐条发送并打印
    std::string line;
    while (std::getline(std::cin, line)) {
        kv::Request request;
        if (!to_request(line, request)) {
            continue;
        }
        if (request.cmd == "exit" || request.cmd == "quit") {
            break;
        }
        std::string payload;
        if (!client.request(request, payload)) {
            break;
        }
        std::cout << payload;
        std::cout.flush();
    }
    return 0;
}
