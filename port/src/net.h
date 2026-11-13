// Minimal TCP transport used by the network foundation.
//
// The port has no external networking dependency: Windows uses Winsock2
// (linked by CMake) and POSIX systems use the BSD socket API.  Messages are
// length-prefixed frames so a higher layer never has to care about partial
// reads or writes.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace buraco::net {

// Initialises the platform socket layer.  Safe to call more than once.
bool init(std::string* err = nullptr);

// Releases the platform socket layer (mainly for symmetry; the process exit
// also cleans it up).
void shutdown();

enum class IoStatus { Ok, Timeout, Closed, Error };

// Largest accepted frame.  Game snapshots/actions are tiny; the cap keeps a
// corrupt peer from asking for a huge allocation.
inline constexpr std::uint32_t kMaxFrame = 1u << 20;  // 1 MiB

class Socket {
public:
    Socket() = default;
    ~Socket();
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    bool valid() const { return handle_ != -1; }
    void close();

    // Binds to `port` on all interfaces and starts listening.  Port 0 asks the
    // system for an ephemeral port (use localPort() to discover it).
    bool listen(std::uint16_t port, std::string* err = nullptr);

    // Waits up to `timeoutMs` for a connection; a negative timeout blocks.
    // Returns no value on timeout or error (check `err` to tell them apart).
    std::optional<Socket> accept(int timeoutMs, std::string* err = nullptr);

    // Connects to `host` (name or literal address).  A negative timeout uses
    // the platform default.  On failure the socket stays invalid.
    bool connect(const std::string& host, std::uint16_t port, int timeoutMs,
                 std::string* err = nullptr);

    // Non-blocking connect used by the online screens.  `beginConnect` starts
    // the attempt; `pollConnect` reports Ok when it completed (or Timeout
    // while still pending).  The socket is left in non-blocking mode.
    bool beginConnect(const std::string& host, std::uint16_t port,
                      std::string* err = nullptr);
    IoStatus pollConnect(std::string* err = nullptr);

    // Non-blocking mode for FrameReader/Session polling.
    bool setNonBlocking(bool nonBlocking);

    std::uint16_t localPort(std::string* err = nullptr) const;

    // Socket-level timeouts, honoured by sendAll/recvAll.  A timeout in the
    // middle of a frame leaves the stream unsynchronised, so callers should
    // treat that as fatal for the connection.
    void setRecvTimeout(int ms);
    void setSendTimeout(int ms);

    IoStatus sendAll(const void* data, std::size_t size, std::string* err = nullptr);
    IoStatus recvAll(void* data, std::size_t size, std::string* err = nullptr);

    // Reads whatever is available without waiting.  Returns Timeout with
    // `received == 0` when the socket would block.
    IoStatus recvSome(void* data, std::size_t capacity, std::size_t& received,
                      std::string* err = nullptr);

    // Writes whatever the socket accepts without waiting.  Returns Timeout
    // with `sent == 0` when the buffer is full; the caller keeps the rest.
    IoStatus sendSome(const void* data, std::size_t size, std::size_t& sent,
                      std::string* err = nullptr);

private:
    explicit Socket(std::intptr_t handle) : handle_(handle) {}
    std::intptr_t handle_ = -1;
    bool connecting_ = false;
};

// Sends one length-prefixed frame (4-byte big-endian length + payload).
bool sendMessage(Socket& socket, const std::string& payload,
                 std::string* err = nullptr);

// Receives one frame.  On IoStatus::Error the connection should be closed.
IoStatus recvMessage(Socket& socket, std::string& payload,
                     std::string* err = nullptr);

// Incremental frame parser for a non-blocking socket.  Feed it every frame;
// complete messages accumulate in `messages`.  A partial frame stays buffered
// between calls, so a message split across TCP reads is reassembled.
class FrameReader {
public:
    IoStatus read(Socket& socket, std::vector<std::string>& messages,
                  std::string* err = nullptr);
    void clear() { buffer_.clear(); }

private:
    std::string buffer_;
};

}  // namespace buraco::net
