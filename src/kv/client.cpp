#include "kv/client.hpp"
#include "kv/net.hpp"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace kv {

Request cmd_put(std::string key, std::string value)
{
    return Request{"put", {std::move(key), std::move(value)}};
}

Request cmd_get(std::string key)
{
    return Request{"get", {std::move(key)}};
}

Request cmd_del(std::string key)
{
    return Request{"del", {std::move(key)}};
}

Request cmd_clr()
{
    return Request{"clr", {}};
}

Request cmd_lst()
{
    return Request{"lst", {}};
}

Request cmd_help()
{
    return Request{"help", {}};
}

Client::~Client()
{
    close();
}

bool Client::connect(const std::string& host, unsigned short port)
{
    close();
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        return false;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        close();
        return false;
    }
    if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close();
        return false;
    }
    return true;
}

void Client::close()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool Client::is_open() const
{
    return fd_ >= 0;
}

// 发送请求帧
bool Client::send(const Request& request)
{
    if (fd_ < 0) {
        return false;
    }
    const std::string body = encode_request(request.cmd, request.args);
    if (body.empty()) {
        return false;
    }
    return write_frame(fd_, body);
}

// 接收响应帧
bool Client::recv(std::string& payload)
{
    if (fd_ < 0) {
        return false;
    }
    return read_frame(fd_, payload);
}

bool Client::send_batch(const std::vector<Request>& requests)
{
    if (fd_ < 0) {
        return false;
    }
    std::string buf;
    for (const Request& r : requests) {
        const std::string body = encode_request(r.cmd, r.args);
        if (body.empty() || !append_frame(buf, body)) {
            return false;
        }
    }
    return send_all(fd_, buf);
}

bool Client::recv_batch(std::size_t n, std::vector<std::string>& out)
{
    out.clear();
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        std::string payload;
        if (!recv(payload)) {
            return false;
        }
        out.push_back(std::move(payload));
    }
    return true;
}

bool Client::request(const Request& request, std::string& payload)
{
    return send(request) && recv(payload);
}

bool Client::request_batch(const std::vector<Request>& requests,
                           std::vector<std::string>& out)
{
    return send_batch(requests) && recv_batch(requests.size(), out);
}

}  // namespace kv
