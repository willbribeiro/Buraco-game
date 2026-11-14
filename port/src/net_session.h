// Non-blocking host/client session used by the online screens.
//
// The session only moves bytes and decoded actions; the authoritative Game
// stays in main.cpp.  The wire flow is:
//
//   host                              client
//   listen(port)                      beginConnect(host, port)
//   <- HELLO <client>                 -> HELLO <host>
//   -> SEAT 1
//   -> START <seed>                   <- (waits for it)
//   -> ACT <action>                   <- host's own action
//   <- ACT <action>                   -> request; host validates and relays
//   -> FP <fingerprint>               <- compares with its own state
//
// The host is seat 0 and the client seat 1.  Only the host advances the hand
// after the score screen.  No thread is involved: poll() is called once per
// frame from the main loop.
#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "net.h"
#include "net_protocol.h"

namespace buraco::net {

enum class SessionRole { None, Host, Client };

enum class SessionState {
    Idle,
    Listening,   // host waiting for a client
    Connecting,  // client resolving/connecting
    Handshake,   // TCP up, exchanging HELLO/SEAT/START
    Ready,       // peer seated; actions can flow
    Closed,      // connection ended
    Error,       // setup or protocol failure
};

struct SessionEvents {
    bool connected = false;     // handshake reached the peer
    bool started = false;       // client received START (host sets it directly)
    bool disconnected = false;  // a connected peer went away
    std::vector<Action> actions;  // host: client requests; client: relayed actions
    std::string fingerprint;      // latest FP received (client)
    std::string error;            // fatal setup error or remote ERR text
};

class Session {
public:
    Session() = default;
    ~Session() = default;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // Host starts listening on all interfaces; port 0 picks an ephemeral port
    // (tests use localPort()).  Returns false and fills `err` on failure.
    bool host(std::uint16_t port, const std::string& name,
              std::string* err = nullptr);

    // Client starts a non-blocking connect.  poll() completes it.
    bool join(const std::string& address, std::uint16_t port,
              const std::string& name, std::string* err = nullptr);

    void close();

    SessionRole role() const { return role_; }
    SessionState state() const { return state_; }
    bool started() const { return started_; }
    bool connected() const { return connected_; }
    int localSeat() const { return role_ == SessionRole::Client ? 1 : 0; }
    int remoteSeat() const { return 1 - localSeat(); }
    std::uint16_t localPort() const { return localPort_; }
    std::uint32_t seed() const { return seed_; }
    const std::string& hostName() const { return hostName_; }
    const std::string& clientName() const { return clientName_; }
    const std::string& error() const { return error_; }

    // Host: sends START and marks the match started.  Both peers then build
    // Game with hostName(), seed() and clientName().
    bool start(std::uint32_t seed, std::string* err = nullptr);

    // Pump the socket.  Never blocks.  Events are cleared first.
    void poll(SessionEvents& events);

    // Client sends its requested action; the host echoes it after validating.
    bool request(const Action& action, std::string* err = nullptr);

    // Host sends an accepted action (its own or a validated remote one).
    bool relay(const Action& action, std::string* err = nullptr);

    // Host sends the state fingerprint after relaying an action.
    bool sendFingerprint(const std::string& text, std::string* err = nullptr);

    // Either side reports a refused action to the peer.
    bool sendError(const std::string& text, std::string* err = nullptr);

private:
    void handleMessage(const std::string& message, SessionEvents& events);
    bool queueMessage(const std::string& payload);
    bool flush(std::string* err = nullptr);
    void failConnection(const std::string& why, SessionEvents& events);
    void resetPeer();

    SessionRole role_ = SessionRole::None;
    SessionState state_ = SessionState::Idle;
    Socket listener_;
    Socket peer_;
    FrameReader reader_;
    std::string localName_;
    std::string hostName_;
    std::string clientName_;
    std::string outbox_;
    std::size_t outOffset_ = 0;
    std::uint32_t seed_ = 0;
    std::uint16_t localPort_ = 0;
    std::string error_;
    bool connected_ = false;
    bool started_ = false;
    bool helloSeen_ = false;
    bool seatSeen_ = false;
    std::chrono::steady_clock::time_point deadline_;
};

}  // namespace buraco::net
