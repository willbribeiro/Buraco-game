#include "net.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#endif

namespace buraco::net {
namespace {

#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr NativeSocket kInvalidSocket = INVALID_SOCKET;

std::once_flag g_wsaOnce;
bool g_wsaReady = false;
std::string g_wsaError;

int lastSocketError() { return WSAGetLastError(); }
std::string socketErrorText(int code) {
    return "winsock error " + std::to_string(code);
}
bool isInterrupted(int code) { return code == WSAEINTR; }
bool isTimeout(int code) {
    return code == WSAETIMEDOUT || code == WSAEWOULDBLOCK;
}
bool isConnectPending(int code) {
    return code == WSAEWOULDBLOCK || code == WSAEINPROGRESS;
}
#else
using NativeSocket = int;
constexpr NativeSocket kInvalidSocket = -1;

int lastSocketError() { return errno; }
std::string socketErrorText(int code) { return std::strerror(code); }
bool isInterrupted(int code) { return code == EINTR; }
bool isTimeout(int code) {
    return code == EAGAIN || code == EWOULDBLOCK || code == ETIMEDOUT;
}
bool isConnectPending(int code) {
    return code == EINPROGRESS || code == EWOULDBLOCK;
}
#endif

NativeSocket native(std::intptr_t handle) { return NativeSocket(handle); }

bool ensureStarted(std::string* err) {
#ifdef _WIN32
    std::call_once(g_wsaOnce, [] {
        WSADATA data{};
        const int rc = WSAStartup(MAKEWORD(2, 2), &data);
        if (rc == 0)
            g_wsaReady = true;
        else
            g_wsaError = "WSAStartup: " + std::to_string(rc);
    });
    if (!g_wsaReady) {
        if (err) *err = g_wsaError;
        return false;
    }
#endif
    if (err) err->clear();
    return true;
}

void closeNative(NativeSocket handle) {
    if (handle == kInvalidSocket) return;
#ifdef _WIN32
    closesocket(handle);
#else
    ::close(handle);
#endif
}

bool setBlocking(NativeSocket handle, bool blocking) {
#ifdef _WIN32
    u_long mode = blocking ? 0 : 1;
    return ioctlsocket(handle, FIONBIO, &mode) == 0;
#else
    const int flags = fcntl(handle, F_GETFL, 0);
    if (flags < 0) return false;
    const int next = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    return fcntl(handle, F_SETFL, next) == 0;
#endif
}

bool setOption(NativeSocket handle, int level, int name, const void* value,
               std::size_t size) {
#ifdef _WIN32
    return setsockopt(handle, level, name, static_cast<const char*>(value),
                      int(size)) == 0;
#else
    return setsockopt(handle, level, name, value, size) == 0;
#endif
}

std::string prefixed(const char* what, int code) {
    return std::string(what) + ": " + socketErrorText(code);
}

}  // namespace

bool init(std::string* err) { return ensureStarted(err); }

Socket::~Socket() { close(); }

Socket::Socket(Socket&& other) noexcept
    : handle_(other.handle_), connecting_(other.connecting_) {
    other.handle_ = -1;
    other.connecting_ = false;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        connecting_ = other.connecting_;
        other.handle_ = -1;
        other.connecting_ = false;
    }
    return *this;
}

void Socket::close() {
    if (handle_ != -1) {
        closeNative(native(handle_));
        handle_ = -1;
    }
    connecting_ = false;
}

bool Socket::listen(std::uint16_t port, std::string* err) {
    if (!ensureStarted(err)) return false;
    close();

    const NativeSocket handle = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (handle == kInvalidSocket) {
        if (err) *err = prefixed("socket", lastSocketError());
        return false;
    }
    handle_ = std::intptr_t(handle);

    int reuse = 1;
    setOption(handle, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (::bind(handle, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) {
        if (err) *err = prefixed("bind", lastSocketError());
        close();
        return false;
    }
    if (::listen(handle, SOMAXCONN) != 0) {
        if (err) *err = prefixed("listen", lastSocketError());
        close();
        return false;
    }
    return true;
}

std::optional<Socket> Socket::accept(int timeoutMs, std::string* err) {
    if (!valid()) {
        if (err) *err = "socket is not listening";
        return std::nullopt;
    }
    const NativeSocket handle = native(handle_);

    if (timeoutMs >= 0) {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(handle, &readable);
        timeval tv{};
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
#ifdef _WIN32
        const int rc = ::select(0, &readable, nullptr, nullptr, &tv);
#else
        const int rc = ::select(handle + 1, &readable, nullptr, nullptr, &tv);
#endif
        if (rc == 0) {
            if (err) *err = "accept timeout";
            return std::nullopt;
        }
        if (rc < 0) {
            if (err) *err = prefixed("select", lastSocketError());
            return std::nullopt;
        }
    }

    sockaddr_in addr{};
#ifdef _WIN32
    int len = sizeof addr;
#else
    socklen_t len = sizeof addr;
#endif
    const NativeSocket peer =
        ::accept(handle, reinterpret_cast<sockaddr*>(&addr), &len);
    if (peer == kInvalidSocket) {
        if (err) *err = prefixed("accept", lastSocketError());
        return std::nullopt;
    }
    // Small turn-based messages (HELLO/SEAT/START/ACT) must not wait for
    // Nagle's algorithm, which can hold a segment until the peer ACKs the
    // previous one.
    int nodelay = 1;
    setOption(peer, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof nodelay);
    if (err) err->clear();
    return Socket(std::intptr_t(peer));
}

bool Socket::connect(const std::string& host, std::uint16_t port,
                     int timeoutMs, std::string* err) {
    if (!ensureStarted(err)) return false;
    close();

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* results = nullptr;
    const std::string service = std::to_string(port);
    const int rc = ::getaddrinfo(host.c_str(), service.c_str(), &hints, &results);
    if (rc != 0 || results == nullptr) {
        if (err) *err = "resolve " + host + ": " + std::to_string(rc);
        if (results) ::freeaddrinfo(results);
        return false;
    }

    bool connected = false;
    for (addrinfo* ai = results; ai != nullptr && !connected; ai = ai->ai_next) {
        const NativeSocket handle =
            ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (handle == kInvalidSocket) continue;

        bool ok = false;
        if (timeoutMs < 0) {
            ok = ::connect(handle, ai->ai_addr,
#ifdef _WIN32
                           int(ai->ai_addrlen)
#else
                           ai->ai_addrlen
#endif
                           ) == 0;
        } else {
            if (setBlocking(handle, false)) {
                if (::connect(handle, ai->ai_addr,
#ifdef _WIN32
                              int(ai->ai_addrlen)
#else
                              ai->ai_addrlen
#endif
                              ) == 0) {
                    ok = true;
                } else {
                    const int code = lastSocketError();
                    if (isConnectPending(code)) {
                        fd_set writable;
                        FD_ZERO(&writable);
                        FD_SET(handle, &writable);
                        timeval tv{};
                        tv.tv_sec = timeoutMs / 1000;
                        tv.tv_usec = (timeoutMs % 1000) * 1000;
#ifdef _WIN32
                        const int ready = ::select(0, nullptr, &writable, nullptr, &tv);
#else
                        const int ready = ::select(handle + 1, nullptr, &writable,
                                                   nullptr, &tv);
#endif
                        if (ready > 0) {
                            int soError = 0;
#ifdef _WIN32
                            int length = sizeof soError;
#else
                            socklen_t length = sizeof soError;
#endif
                            if (getsockopt(handle, SOL_SOCKET, SO_ERROR,
                                           reinterpret_cast<char*>(&soError),
                                           &length) == 0)
                                ok = soError == 0;
                        }
                    }
                }
                setBlocking(handle, true);
            }
        }

        if (ok) {
            int nodelay = 1;
            setOption(handle, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof nodelay);
            handle_ = std::intptr_t(handle);
            connected = true;
        } else {
            closeNative(handle);
        }
    }

    ::freeaddrinfo(results);
    if (!connected) {
        if (err) *err = "connect " + host + ":" + service + " failed";
        return false;
    }
    connecting_ = false;
    if (err) err->clear();
    return true;
}

bool Socket::beginConnect(const std::string& host, std::uint16_t port,
                          std::string* err) {
    if (!ensureStarted(err)) return false;
    close();

    addrinfo hints{};
    hints.ai_family = AF_INET;  // the host listens on an IPv4 socket
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* results = nullptr;
    const std::string service = std::to_string(port);
    const int rc = ::getaddrinfo(host.c_str(), service.c_str(), &hints, &results);
    if (rc != 0 || results == nullptr) {
        if (err) *err = "resolve " + host + ": " + std::to_string(rc);
        if (results) ::freeaddrinfo(results);
        return false;
    }

    bool pending = false;
    for (addrinfo* ai = results; ai != nullptr; ai = ai->ai_next) {
        const NativeSocket handle =
            ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (handle == kInvalidSocket) continue;
        int nodelay = 1;
        setOption(handle, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof nodelay);
        if (!setBlocking(handle, false)) {
            closeNative(handle);
            continue;
        }
        const int opened = ::connect(handle, ai->ai_addr,
#ifdef _WIN32
                                     int(ai->ai_addrlen)
#else
                                     ai->ai_addrlen
#endif
        );
        if (opened == 0) {
            handle_ = std::intptr_t(handle);
            connecting_ = false;
            break;
        }
        const int code = lastSocketError();
        if (isConnectPending(code)) {
            handle_ = std::intptr_t(handle);
            connecting_ = true;
            pending = true;
            break;
        }
        closeNative(handle);
    }

    ::freeaddrinfo(results);
    if (!valid()) {
        if (err) *err = "connect " + host + ":" + service + " failed";
        return false;
    }
    if (!pending) connecting_ = false;
    if (err) err->clear();
    return true;
}

IoStatus Socket::pollConnect(std::string* err) {
    if (!valid()) {
        if (err) *err = "socket is not connecting";
        return IoStatus::Error;
    }
    if (!connecting_) return IoStatus::Ok;

    fd_set writable;
    FD_ZERO(&writable);
    FD_SET(native(handle_), &writable);
    timeval tv{};
#ifdef _WIN32
    const int ready = ::select(0, nullptr, &writable, nullptr, &tv);
#else
    const int ready = ::select(native(handle_) + 1, nullptr, &writable, nullptr, &tv);
#endif
    if (ready == 0) return IoStatus::Timeout;
    if (ready < 0) {
        if (err) *err = prefixed("select", lastSocketError());
        close();
        return IoStatus::Error;
    }

    int soError = 0;
#ifdef _WIN32
    int length = sizeof soError;
#else
    socklen_t length = sizeof soError;
#endif
    if (getsockopt(native(handle_), SOL_SOCKET, SO_ERROR,
                   reinterpret_cast<char*>(&soError), &length) != 0) {
        if (err) *err = prefixed("getsockopt", lastSocketError());
        close();
        return IoStatus::Error;
    }
    if (soError != 0) {
        if (err) *err = prefixed("connect", soError);
        close();
        return IoStatus::Error;
    }
    connecting_ = false;
    return IoStatus::Ok;
}

bool Socket::setNonBlocking(bool nonBlocking) {
    return valid() && setBlocking(native(handle_), !nonBlocking);
}

std::uint16_t Socket::localPort(std::string* err) const {
    if (!valid()) {
        if (err) *err = "socket is not bound";
        return 0;
    }
    sockaddr_in addr{};
#ifdef _WIN32
    int len = sizeof addr;
#else
    socklen_t len = sizeof addr;
#endif
    if (getsockname(native(handle_), reinterpret_cast<sockaddr*>(&addr), &len) !=
        0) {
        if (err) *err = prefixed("getsockname", lastSocketError());
        return 0;
    }
    if (err) err->clear();
    return ntohs(addr.sin_port);
}

void Socket::setRecvTimeout(int ms) {
    if (!valid()) return;
#ifdef _WIN32
    DWORD value = DWORD(ms < 0 ? 0 : ms);
    setOption(native(handle_), SOL_SOCKET, SO_RCVTIMEO, &value, sizeof value);
#else
    timeval tv{};
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    setOption(native(handle_), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
#endif
}

IoStatus Socket::sendAll(const void* data, std::size_t size, std::string* err) {
    if (!valid()) {
        if (err) *err = "socket is not connected";
        return IoStatus::Error;
    }
    const char* bytes = static_cast<const char*>(data);
    std::size_t sent = 0;
    while (sent < size) {
#ifdef _WIN32
        const int n = ::send(native(handle_), bytes + sent,
                             int(std::min<std::size_t>(size - sent, kMaxFrame)),
                             0);
#else
        const long n = long(::send(native(handle_), bytes + sent, size - sent,
                                   MSG_NOSIGNAL));
#endif
        if (n > 0) {
            sent += std::size_t(n);
            continue;
        }
        if (n == 0) {
            if (err) *err = "connection closed";
            return IoStatus::Closed;
        }
        const int code = lastSocketError();
        if (isInterrupted(code)) continue;
        if (isTimeout(code)) {
            if (err) *err = "send timeout";
            return IoStatus::Timeout;
        }
        if (err) *err = prefixed("send", code);
        return IoStatus::Error;
    }
    return IoStatus::Ok;
}

IoStatus Socket::recvAll(void* data, std::size_t size, std::string* err) {
    if (!valid()) {
        if (err) *err = "socket is not connected";
        return IoStatus::Error;
    }
    char* bytes = static_cast<char*>(data);
    std::size_t got = 0;
    while (got < size) {
#ifdef _WIN32
        const int n = ::recv(native(handle_), bytes + got,
                             int(std::min<std::size_t>(size - got, kMaxFrame)),
                             0);
#else
        const long n = long(::recv(native(handle_), bytes + got, size - got, 0));
#endif
        if (n > 0) {
            got += std::size_t(n);
            continue;
        }
        if (n == 0) {
            if (err) *err = "connection closed";
            return IoStatus::Closed;
        }
        const int code = lastSocketError();
        if (isInterrupted(code)) continue;
        if (isTimeout(code)) {
            if (err) *err = "receive timeout";
            return IoStatus::Timeout;
        }
        if (err) *err = prefixed("recv", code);
        return IoStatus::Error;
    }
    return IoStatus::Ok;
}

IoStatus Socket::sendSome(const void* data, std::size_t size,
                          std::size_t& sent, std::string* err) {
    sent = 0;
    if (!valid()) {
        if (err) *err = "socket is not connected";
        return IoStatus::Error;
    }
    if (size == 0) return IoStatus::Ok;
    for (;;) {
#ifdef _WIN32
        const int n = ::send(native(handle_), static_cast<const char*>(data),
                             int(std::min<std::size_t>(size, kMaxFrame)), 0);
#else
        const long n =
            long(::send(native(handle_), data, size, MSG_NOSIGNAL));
#endif
        if (n > 0) {
            sent = std::size_t(n);
            return IoStatus::Ok;
        }
        if (n == 0) {
            if (err) *err = "connection closed";
            return IoStatus::Closed;
        }
        const int code = lastSocketError();
        if (isInterrupted(code)) continue;
        if (isTimeout(code)) return IoStatus::Timeout;
        if (err) *err = prefixed("send", code);
        return IoStatus::Error;
    }
}

IoStatus Socket::recvSome(void* data, std::size_t capacity,
                          std::size_t& received, std::string* err) {
    received = 0;
    if (!valid()) {
        if (err) *err = "socket is not connected";
        return IoStatus::Error;
    }
    if (capacity == 0) return IoStatus::Ok;
    for (;;) {
#ifdef _WIN32
        const int n = ::recv(native(handle_), static_cast<char*>(data),
                             int(std::min<std::size_t>(capacity, kMaxFrame)), 0);
#else
        const long n = long(::recv(native(handle_), data, capacity, 0));
#endif
        if (n > 0) {
            received = std::size_t(n);
            return IoStatus::Ok;
        }
        if (n == 0) {
            if (err) *err = "connection closed";
            return IoStatus::Closed;
        }
        const int code = lastSocketError();
        if (isInterrupted(code)) continue;
        if (isTimeout(code)) return IoStatus::Timeout;
        if (err) *err = prefixed("recv", code);
        return IoStatus::Error;
    }
}

bool sendMessage(Socket& socket, const std::string& payload, std::string* err) {
    if (payload.size() > kMaxFrame) {
        if (err) *err = "frame too large";
        return false;
    }
    const std::uint32_t size = std::uint32_t(payload.size());
    const unsigned char header[kFrameHeaderSize] = {
        static_cast<unsigned char>((size >> 24) & 0xFF),
        static_cast<unsigned char>((size >> 16) & 0xFF),
        static_cast<unsigned char>((size >> 8) & 0xFF),
        static_cast<unsigned char>(size & 0xFF),
    };
    if (socket.sendAll(header, sizeof header, err) != IoStatus::Ok) return false;
    if (!payload.empty() &&
        socket.sendAll(payload.data(), payload.size(), err) != IoStatus::Ok)
        return false;
    return true;
}

IoStatus recvMessage(Socket& socket, std::string& payload, std::string* err) {
    payload.clear();
    unsigned char header[kFrameHeaderSize];
    const IoStatus headerStatus = socket.recvAll(header, sizeof header, err);
    if (headerStatus != IoStatus::Ok) return headerStatus;

    const std::uint32_t size = (std::uint32_t(header[0]) << 24) |
                               (std::uint32_t(header[1]) << 16) |
                               (std::uint32_t(header[2]) << 8) |
                               std::uint32_t(header[3]);
    if (size > kMaxFrame) {
        if (err) *err = "frame too large";
        socket.close();
        return IoStatus::Error;
    }
    payload.resize(size);
    if (size == 0) return IoStatus::Ok;
    return socket.recvAll(payload.data(), payload.size(), err);
}

IoStatus FrameReader::read(Socket& socket, std::vector<std::string>& messages,
                           std::string* err) {
    messages.clear();
    bool any = false;
    bool closed = false;
    for (;;) {
        char chunk[4096];
        std::size_t got = 0;
        const IoStatus status = socket.recvSome(chunk, sizeof chunk, got, err);
        if (status == IoStatus::Timeout) break;
        if (status == IoStatus::Closed) {
            closed = true;
            break;
        }
        if (status != IoStatus::Ok) return status;
        if (got == 0) break;
        any = true;
        buffer_.append(chunk, got);
    }

    std::size_t offset = 0;
    while (buffer_.size() - offset >= kFrameHeaderSize) {
        const unsigned char* header =
            reinterpret_cast<const unsigned char*>(buffer_.data() + offset);
        const std::uint32_t size = (std::uint32_t(header[0]) << 24) |
                                   (std::uint32_t(header[1]) << 16) |
                                   (std::uint32_t(header[2]) << 8) |
                                   std::uint32_t(header[3]);
        if (size > kMaxFrame) {
            if (err) *err = "frame too large";
            buffer_.clear();
            socket.close();
            return IoStatus::Error;
        }
        if (buffer_.size() - offset < std::size_t(size) + kFrameHeaderSize) break;
        messages.emplace_back(buffer_, offset + kFrameHeaderSize, size);
        offset += std::size_t(size) + kFrameHeaderSize;
    }
    if (offset > 0) buffer_.erase(0, offset);

    if (closed) {
        if (!buffer_.empty()) {
            if (err) *err = "connection closed mid-frame";
            buffer_.clear();
            return IoStatus::Error;
        }
        return IoStatus::Closed;
    }
    return any ? IoStatus::Ok : IoStatus::Timeout;
}

}  // namespace buraco::net
