#include "kv/net.hpp"

#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>

namespace kv {

bool read_line(int fd, std::string& out)
{
    out.clear();
    char c;
    for (;;) {
        const ssize_t n = ::recv(fd, &c, 1, 0);
        if (n == 1) {
            if (c == '\n') {
                return true;
            }
            out.push_back(c);
            if (out.size() > kMaxLine) {
                return false;
            }
        }
        else if (n == 0) {
            return false;  // EOF
        }
        else if (errno == EINTR) {
            continue;
        }
        else {
            return false;  // 出错
        }
    }
}

bool send_all(int fd, const std::string& data)
{
    std::size_t sent = 0;
    while (sent < data.size()) {
        const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if (n > 0) {
            sent += static_cast<std::size_t>(n);
        }
        else if (n < 0 && errno == EINTR) {
            continue;
        }
        else {
            return false;
        }
    }
    return true;
}

bool recv_frame(int fd, std::string& out)
{
    out.clear();
    std::string line;
    while (read_line(fd, line)) {
        if (line.empty()) {
            return true;  // 空行帧尾
        }
        out += line;
        out += '\n';
    }
    return false;
}

}  // namespace kv
