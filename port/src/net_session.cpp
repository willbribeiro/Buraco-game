#include "net_session.h"

#include "i18n.h"

#include <utility>

namespace buraco::net {
namespace {

using Clock = std::chrono::steady_clock;

constexpr auto kHandshakeTimeout = std::chrono::seconds(10);

// Let a few frames queue while the peer is slow, and compact the consumed
// prefix once it grows past this many bytes.
constexpr std::size_t kMaxQueuedFrames = 4;
constexpr std::size_t kMaxOutboxBytes = kMaxQueuedFrames * std::size_t(kMaxFrame);
constexpr std::size_t kOutboxCompactThreshold = 8192;

std::string cleanError(const std::string& text) {
    return text.empty() ? std::string(i18n::text(i18n::Text::NetFailure)) : text;
}

}  // namespace

bool Session::host(std::uint16_t port, const std::string& name,
                   std::string* err) {
    close();
    if (!net::init(err)) {
        state_ = SessionState::Error;
        error_ = cleanError(err ? *err : std::string());
        return false;
    }
    if (!listener_.listen(port, err)) {
        state_ = SessionState::Error;
        error_ = cleanError(err ? *err : std::string());
        return false;
    }
    localPort_ = listener_.localPort();
    if (localPort_ == 0) {
        if (err) *err = i18n::text(i18n::Text::NetNoPort);
        state_ = SessionState::Error;
        error_ = *err;
        listener_.close();
        return false;
    }

    role_ = SessionRole::Host;
    state_ = SessionState::Listening;
    localName_ = name.empty() ? i18n::text(i18n::Text::DefaultPlayer1) : name;
    hostName_ = localName_;
    clientName_.clear();
    if (err) err->clear();
    return true;
}

bool Session::join(const std::string& address, std::uint16_t port,
                   const std::string& name, std::string* err) {
    close();
    if (!net::init(err)) {
        state_ = SessionState::Error;
        error_ = cleanError(err ? *err : std::string());
        return false;
    }
    if (!peer_.beginConnect(address, port, err)) {
        state_ = SessionState::Error;
        error_ = cleanError(err ? *err : std::string());
        return false;
    }

    role_ = SessionRole::Client;
    state_ = SessionState::Connecting;
    localName_ = name.empty() ? i18n::text(i18n::Text::DefaultPlayer2) : name;
    clientName_ = localName_;
    hostName_.clear();
    deadline_ = Clock::now() + kHandshakeTimeout;
    if (err) err->clear();
    return true;
}

void Session::close() {
    listener_.close();
    peer_.close();
    reader_.clear();
    outbox_.clear();
    outOffset_ = 0;
    role_ = SessionRole::None;
    state_ = SessionState::Idle;
    localName_.clear();
    hostName_.clear();
    clientName_.clear();
    seed_ = 0;
    localPort_ = 0;
    error_.clear();
    connected_ = false;
    started_ = false;
    helloSeen_ = false;
    seatSeen_ = false;
}

bool Session::queueMessage(const std::string& payload) {
    if (payload.size() > kMaxFrame) {
        error_ = i18n::text(i18n::Text::NetMessageTooLarge);
        return false;
    }
    // Keep a slow peer from making the process grow without bound.
    if (outbox_.size() - outOffset_ + payload.size() + kFrameHeaderSize >
        kMaxOutboxBytes) {
        error_ = i18n::text(i18n::Text::NetSendQueueFull);
        return false;
    }
    const std::uint32_t size = std::uint32_t(payload.size());
    const unsigned char header[4] = {
        static_cast<unsigned char>((size >> 24) & 0xFF),
        static_cast<unsigned char>((size >> 16) & 0xFF),
        static_cast<unsigned char>((size >> 8) & 0xFF),
        static_cast<unsigned char>(size & 0xFF),
    };
    outbox_.append(reinterpret_cast<const char*>(header), sizeof header);
    outbox_ += payload;
    return true;
}

bool Session::flush(std::string* err) {
    if (!peer_.valid()) {
        if (err) *err = i18n::text(i18n::Text::NetNoConnection);
        return false;
    }
    while (outOffset_ < outbox_.size()) {
        std::size_t sent = 0;
        std::string why;
        const IoStatus status =
            peer_.sendSome(outbox_.data() + outOffset_, outbox_.size() - outOffset_,
                           sent, &why);
        if (status == IoStatus::Ok) {
            if (sent == 0) break;
            outOffset_ += sent;
            continue;
        }
        if (status == IoStatus::Timeout) break;  // keep the rest for next poll
        if (err) *err = cleanError(why);
        return false;
    }
    if (outOffset_ == outbox_.size()) {
        outbox_.clear();
        outOffset_ = 0;
    } else if (outOffset_ > kOutboxCompactThreshold) {
        outbox_.erase(0, outOffset_);
        outOffset_ = 0;
    }
    return true;
}

void Session::resetPeer() {
    peer_.close();
    reader_.clear();
    outbox_.clear();
    outOffset_ = 0;
    connected_ = false;
    helloSeen_ = false;
    seatSeen_ = false;
}

void Session::failConnection(const std::string& why, SessionEvents& events) {
    const bool wasConnected = connected_;
    const bool wasStarted = started_;
    error_ = cleanError(why);
    if (events.error.empty()) events.error = error_;
    if (wasConnected) events.disconnected = true;

    resetPeer();
    if (role_ == SessionRole::Host && !wasStarted && listener_.valid()) {
        // Keep the port open so another client can take the seat.
        state_ = SessionState::Listening;
        started_ = false;
        seed_ = 0;
        return;
    }
    started_ = wasStarted;
    state_ = SessionState::Closed;
}

void Session::handleMessage(const std::string& message, SessionEvents& events) {
    const std::string tag = tagOf(message);
    if (tag.empty()) {
        failConnection(i18n::text(i18n::Text::NetInvalidMessage), events);
        return;
    }
    std::string err;

    if (tag == "HELLO") {
        std::string name;
        if (!parseHello(message, name, &err)) {
            failConnection(i18n::format(i18n::Text::NetInvalidHello, err.c_str()),
                           events);
            return;
        }
        if (role_ == SessionRole::Host) {
            if (connected_) return;  // duplicate, ignore
            clientName_ =
                name.empty() ? i18n::text(i18n::Text::DefaultPlayer2) : name;
            hostName_ = localName_;
            connected_ = true;
            state_ = SessionState::Ready;
            if (!queueMessage(makeSeat(1))) {
                failConnection(error_, events);
                return;
            }
            events.connected = true;
        } else {
            if (helloSeen_) return;
            hostName_ = name.empty() ? i18n::text(i18n::Text::DefaultPlayer1) : name;
            helloSeen_ = true;
            if (seatSeen_ && !connected_) {
                connected_ = true;
                events.connected = true;
            }
        }
        return;
    }

    if (tag == "SEAT") {
        if (role_ != SessionRole::Client) {
            failConnection(i18n::text(i18n::Text::NetUnexpectedSeat), events);
            return;
        }
        int seat = -1;
        if (!parseSeat(message, seat, &err)) {
            failConnection(i18n::format(i18n::Text::NetInvalidSeat, err.c_str()),
                           events);
            return;
        }
        if (seat != 1) {
            failConnection(i18n::text(i18n::Text::NetUnexpectedRemoteSeat), events);
            return;
        }
        seatSeen_ = true;
        if (helloSeen_ && !connected_) {
            connected_ = true;
            events.connected = true;
        }
        return;
    }

    if (tag == "START") {
        if (role_ != SessionRole::Client) {
            failConnection(i18n::text(i18n::Text::NetUnexpectedStart), events);
            return;
        }
        if (started_) return;
        std::uint32_t value = 0;
        if (!parseStart(message, value, &err)) {
            failConnection(i18n::format(i18n::Text::NetInvalidStart, err.c_str()),
                           events);
            return;
        }
        if (!seatSeen_) {
            failConnection(i18n::text(i18n::Text::NetStartBeforeSeat), events);
            return;
        }
        seed_ = value;
        started_ = true;
        state_ = SessionState::Ready;
        events.started = true;
        return;
    }

    if (tag == "ACT") {
        if (!started_) {
            failConnection(i18n::text(i18n::Text::NetActionBeforeStart), events);
            return;
        }
        Action action;
        if (!parseAction(message, action, &err)) {
            failConnection(i18n::format(i18n::Text::NetInvalidAction, err.c_str()),
                           events);
            return;
        }
        events.actions.push_back(std::move(action));
        return;
    }

    if (tag == "FP") {
        std::string text;
        if (!parseFingerprint(message, text, &err)) {
            failConnection(
                i18n::format(i18n::Text::NetInvalidFingerprint, err.c_str()), events);
            return;
        }
        events.fingerprint = text;
        return;
    }

    if (tag == "ERR") {
        std::string text;
        if (!parseError(message, text, &err)) {
            failConnection(i18n::format(i18n::Text::NetInvalidErr, err.c_str()), events);
            return;
        }
        // A refused action is reported by the host but is not fatal.
        events.error = text.empty() ? i18n::text(i18n::Text::NetActionRefused) : text;
        return;
    }

    failConnection(i18n::format(i18n::Text::NetUnexpectedMessage, tag.c_str()), events);
}

void Session::poll(SessionEvents& events) {
    events = SessionEvents{};
    if (role_ == SessionRole::None || state_ == SessionState::Idle ||
        state_ == SessionState::Closed || state_ == SessionState::Error)
        return;

    if (state_ == SessionState::Listening) {
        std::string err;
        auto peer = listener_.accept(0, &err);
        if (!peer) {
            // A zero timeout reports "accept timeout" on every idle frame.
            if (!err.empty() && err != "accept timeout") {
                error_ = err;
                events.error = err;
            }
            return;
        }
        peer_ = std::move(*peer);
        peer_.setNonBlocking(true);
        state_ = SessionState::Handshake;
        connected_ = false;
        started_ = false;
        helloSeen_ = false;
        seatSeen_ = false;
        reader_.clear();
        deadline_ = Clock::now() + kHandshakeTimeout;
        if (!queueMessage(makeHello(localName_))) {
            failConnection(error_, events);
            return;
        }
        if (!flush(&err)) failConnection(err, events);
        return;
    }

    if (state_ == SessionState::Connecting) {
        if (Clock::now() > deadline_) {
            failConnection(i18n::text(i18n::Text::NetConnectTimeout), events);
            return;
        }
        std::string err;
        const IoStatus status = peer_.pollConnect(&err);
        if (status == IoStatus::Timeout) return;
        if (status != IoStatus::Ok) {
            failConnection(err, events);
            return;
        }
        peer_.setNonBlocking(true);
        state_ = SessionState::Handshake;
        deadline_ = Clock::now() + kHandshakeTimeout;
        if (!queueMessage(makeHello(localName_))) {
            failConnection(error_, events);
            return;
        }
        if (!flush(&err)) failConnection(err, events);
        return;
    }

    if (state_ != SessionState::Handshake && state_ != SessionState::Ready) return;
    if (state_ == SessionState::Handshake && !connected_ &&
        Clock::now() > deadline_) {
        failConnection(i18n::text(i18n::Text::NetHandshakeTimeout), events);
        return;
    }

    std::vector<std::string> messages;
    std::string err;
    const bool wasConnected = connected_;
    const IoStatus status = reader_.read(peer_, messages, &err);
    for (const std::string& message : messages) {
        if (state_ == SessionState::Closed || state_ == SessionState::Error)
            break;
        handleMessage(message, events);
    }
    if (state_ != SessionState::Closed && state_ != SessionState::Error) {
        if (!flush(&err)) {
            failConnection(err, events);
            return;
        }
    }

    if (status == IoStatus::Closed) {
        if (wasConnected || connected_) {
            failConnection(i18n::text(i18n::Text::NetConnectionClosed), events);
        } else {
            failConnection(i18n::text(i18n::Text::NetConnectionClosedHandshake),
                           events);
        }
    } else if (status == IoStatus::Error) {
        failConnection(err, events);
    }
}

bool Session::start(std::uint32_t seed, std::string* err) {
    if (role_ != SessionRole::Host || !connected_ ||
        (state_ != SessionState::Ready && state_ != SessionState::Handshake)) {
        if (err) *err = i18n::text(i18n::Text::NetNoClient);
        return false;
    }
    if (started_) return true;
    started_ = true;
    seed_ = seed;
    state_ = SessionState::Ready;
    listener_.close();
    if (!queueMessage(makeStart(seed))) {
        if (err) *err = error_;
        started_ = false;
        state_ = SessionState::Closed;
        return false;
    }
    if (!flush(err)) {
        error_ = cleanError(err ? *err : std::string());
        started_ = false;
        state_ = SessionState::Closed;
        return false;
    }
    return true;
}

bool Session::request(const Action& action, std::string* err) {
    if (role_ != SessionRole::Client || !started_) {
        if (err) *err = i18n::text(i18n::Text::NetNoMatch);
        return false;
    }
    if (!queueMessage(makeAction(action))) {
        if (err) *err = error_;
        return false;
    }
    if (!flush(err)) {
        error_ = cleanError(err ? *err : std::string());
        return false;
    }
    return true;
}

bool Session::relay(const Action& action, std::string* err) {
    if (role_ != SessionRole::Host || !started_) {
        if (err) *err = i18n::text(i18n::Text::NetNoMatch);
        return false;
    }
    if (!queueMessage(makeAction(action))) {
        if (err) *err = error_;
        return false;
    }
    if (!flush(err)) {
        error_ = cleanError(err ? *err : std::string());
        return false;
    }
    return true;
}

bool Session::sendFingerprint(const std::string& text, std::string* err) {
    if (role_ != SessionRole::Host || !started_) {
        if (err) *err = i18n::text(i18n::Text::NetNoMatch);
        return false;
    }
    if (!queueMessage(makeFingerprint(text))) {
        if (err) *err = error_;
        return false;
    }
    if (!flush(err)) {
        error_ = cleanError(err ? *err : std::string());
        return false;
    }
    return true;
}

bool Session::sendError(const std::string& text, std::string* err) {
    if (!connected_) {
        if (err) *err = i18n::text(i18n::Text::NetNotConnected);
        return false;
    }
    if (!queueMessage(makeError(text))) {
        if (err) *err = error_;
        return false;
    }
    if (!flush(err)) {
        error_ = cleanError(err ? *err : std::string());
        return false;
    }
    return true;
}

}  // namespace buraco::net
