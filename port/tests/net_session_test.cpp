// Host/client session tests.  Both ends run in the same thread and are pumped
// alternately, which exercises the non-blocking state machine without relying
// on timing.
#include "net_session.h"

#include "test_framework.hpp"

using namespace buraco;
using namespace buraco::net;

namespace {

struct Pipe {
    Session host;
    Session client;
    SessionEvents he;
    SessionEvents ce;
    bool hostConnected = false;
    bool clientConnected = false;
    bool hostDisconnected = false;
    bool clientDisconnected = false;
    bool clientStarted = false;
    std::vector<Action> hostActions;
    std::vector<Action> clientActions;
    std::vector<std::string> hostErrors;
    std::vector<std::string> clientErrors;
    std::vector<std::string> fingerprints;

    void step() {
        host.poll(he);
        hostConnected = hostConnected || he.connected;
        hostDisconnected = hostDisconnected || he.disconnected;
        if (!he.error.empty()) hostErrors.push_back(he.error);
        for (const Action& a : he.actions) hostActions.push_back(a);
        client.poll(ce);
        clientConnected = clientConnected || ce.connected;
        clientDisconnected = clientDisconnected || ce.disconnected;
        clientStarted = clientStarted || ce.started;
        if (!ce.error.empty()) clientErrors.push_back(ce.error);
        for (const Action& a : ce.actions) clientActions.push_back(a);
        if (!ce.fingerprint.empty()) fingerprints.push_back(ce.fingerprint);
    }

    bool connected(int rounds = 500) {
        for (int i = 0; i < rounds && !(hostConnected && clientConnected); ++i)
            step();
        return hostConnected && clientConnected;
    }

    bool started(int rounds = 500) {
        for (int i = 0; i < rounds && !clientStarted; ++i) step();
        return clientStarted;
    }
};

Action actionOf(ActionKind kind) {
    Action a;
    a.kind = kind;
    return a;
}

}  // namespace

TEST_CASE("session handshake assigns host seat 0 and client seat 1") {
    Pipe p;
    std::string err;
    REQUIRE(p.host.host(0, "Ana", &err));
    const std::uint16_t port = p.host.localPort();
    REQUIRE(port != 0);
    REQUIRE(p.client.join("127.0.0.1", port, "Bia", &err));
    REQUIRE(p.connected());

    CHECK(p.host.role() == SessionRole::Host);
    CHECK(p.client.role() == SessionRole::Client);
    CHECK_EQ(p.host.localSeat(), 0);
    CHECK_EQ(p.client.localSeat(), 1);
    CHECK_EQ(p.host.remoteSeat(), 1);
    CHECK_EQ(p.client.remoteSeat(), 0);
    CHECK_EQ(p.host.hostName(), std::string("Ana"));
    CHECK_EQ(p.host.clientName(), std::string("Bia"));
    CHECK_EQ(p.client.hostName(), std::string("Ana"));
    CHECK_EQ(p.client.clientName(), std::string("Bia"));
    CHECK(p.host.connected());
    CHECK(p.client.connected());
    CHECK(!p.host.started());
    CHECK(!p.client.started());

    REQUIRE(p.host.start(4242u, &err));
    CHECK(p.host.started());
    REQUIRE(p.started());
    CHECK(p.client.started());
    CHECK_EQ(p.client.seed(), 4242u);
    CHECK(p.client.state() == SessionState::Ready);

    // Only the host may start a match and only the client may request.
    CHECK(!p.client.start(1u, &err));
    CHECK(!p.host.request(actionOf(ActionKind::DrawStock), &err));
}

TEST_CASE("session relays actions, fingerprints and refusals") {
    Pipe p;
    std::string err;
    REQUIRE(p.host.host(0, "Ana", &err));
    REQUIRE(p.client.join("127.0.0.1", p.host.localPort(), "Bia", &err));
    REQUIRE(p.connected());
    REQUIRE(p.host.start(99u, &err));
    REQUIRE(p.started());

    // Client -> host request.
    Action draw = actionOf(ActionKind::DrawStock);
    REQUIRE(p.client.request(draw, &err));
    for (int i = 0; i < 500 && p.hostActions.empty(); ++i) p.step();
    REQUIRE(p.hostActions.size() == 1);
    CHECK(p.hostActions[0].kind == ActionKind::DrawStock);

    // Host validates, applies and relays it to the client.
    REQUIRE(p.host.relay(p.hostActions[0], &err));
    for (int i = 0; i < 500 && p.clientActions.empty(); ++i) p.step();
    REQUIRE(p.clientActions.size() == 1);
    CHECK(p.clientActions[0].kind == ActionKind::DrawStock);

    // Fingerprint after applying.
    REQUIRE(p.host.sendFingerprint("0123456789abcdef", &err));
    for (int i = 0; i < 500 && p.fingerprints.empty(); ++i) p.step();
    REQUIRE(p.fingerprints.size() == 1);
    CHECK_EQ(p.fingerprints[0], std::string("0123456789abcdef"));

    // A refused action is reported but keeps the connection alive.
    REQUIRE(p.host.sendError("jogada recusada", &err));
    for (int i = 0; i < 500 && p.clientErrors.empty(); ++i) p.step();
    REQUIRE(!p.clientErrors.empty());
    CHECK_EQ(p.clientErrors.back(), std::string("jogada recusada"));
    CHECK(p.client.state() == SessionState::Ready);
    CHECK(!p.clientDisconnected);
}

TEST_CASE("session reports a disconnect after the match started") {
    Pipe p;
    std::string err;
    REQUIRE(p.host.host(0, "Ana", &err));
    REQUIRE(p.client.join("127.0.0.1", p.host.localPort(), "Bia", &err));
    REQUIRE(p.connected());
    REQUIRE(p.host.start(7u, &err));
    REQUIRE(p.started());

    p.client.close();
    for (int i = 0; i < 500 && !p.hostDisconnected; ++i) p.step();

    CHECK(p.hostDisconnected);
    CHECK(p.host.state() == SessionState::Closed);
    CHECK(!p.host.connected());
    CHECK(p.host.started());
}

TEST_CASE("host accepts another client after a handshake drop") {
    Pipe p;
    std::string err;
    REQUIRE(p.host.host(0, "Ana", &err));
    const std::uint16_t port = p.host.localPort();
    REQUIRE(p.client.join("127.0.0.1", port, "Bia", &err));
    REQUIRE(p.connected());

    // Drop before the host starts: the listening socket must stay usable.
    p.client.close();
    for (int i = 0; i < 500 && p.host.state() != SessionState::Listening; ++i)
        p.step();
    CHECK(p.hostDisconnected);
    CHECK(p.host.state() == SessionState::Listening);
    CHECK_EQ(p.host.localPort(), port);

    Pipe second;
    REQUIRE(second.client.join("127.0.0.1", port, "Caio", &err));
    bool hostSawClient = false;
    bool clientSawHost = false;
    for (int i = 0; i < 500 && !(hostSawClient && clientSawHost); ++i) {
        p.host.poll(second.he);
        second.client.poll(second.ce);
        hostSawClient = hostSawClient || second.he.connected;
        clientSawHost = clientSawHost || second.ce.connected;
    }
    CHECK(hostSawClient);
    CHECK(clientSawHost);
    CHECK_EQ(second.client.hostName(), std::string("Ana"));

    REQUIRE(p.host.start(11u, &err));
    // The first pipe's client is gone; the second one receives START.
    bool started = false;
    for (int i = 0; i < 500 && !started; ++i) {
        p.host.poll(second.he);
        second.client.poll(second.ce);
        started = started || second.ce.started;
    }
    CHECK(started);
    CHECK_EQ(second.client.seed(), 11u);
}

namespace {

bool nextAction(Game& game, Action& action) {
    switch (game.phase()) {
        case Phase::Draw:
            if (!game.monte().empty()) {
                action.kind = ActionKind::DrawStock;
            } else if (game.canTakeDiscard()) {
                action.kind = ActionKind::DrawTable;
            } else {
                return false;
            }
            return true;
        case Phase::Play:
            action.kind = ActionKind::RequestDiscard;
            return true;
        case Phase::Discard:
            if (game.player(game.current()).hand.empty()) return false;
            action.kind = ActionKind::Discard;
            action.card = game.player(game.current()).hand.front();
            return true;
        case Phase::HandOver:
            action.kind = ActionKind::Continue;
            return true;
        case Phase::MatchOver:
            return false;
    }
    return false;
}

}  // namespace

TEST_CASE("two games stay in lockstep through the live session") {
    Pipe p;
    std::string err;
    REQUIRE(p.host.host(0, "Ana", &err));
    REQUIRE(p.client.join("127.0.0.1", p.host.localPort(), "Bia", &err));
    REQUIRE(p.connected());
    REQUIRE(p.host.start(2026u, &err));
    REQUIRE(p.started());

    Game hostGame;
    Game clientGame;
    hostGame.newMatch("Ana", 2026u, "Bia");
    clientGame.newMatch("Ana", 2026u, "Bia");
    hostGame.setController(0, Controller::Human);
    hostGame.setController(1, Controller::Remote);
    clientGame.setController(0, Controller::Remote);
    clientGame.setController(1, Controller::Human);
    REQUIRE(fingerprintText(hostGame) == fingerprintText(clientGame));

    int steps = 0;
    for (; steps < 250 && hostGame.phase() != Phase::MatchOver; ++steps) {
        Action action;
        REQUIRE(nextAction(hostGame, action));

        if (hostGame.phase() == Phase::HandOver ||
            hostGame.current() == 0) {
            // Host's own action (or the hand advance): apply, relay, confirm.
            REQUIRE(apply(hostGame, action, &err));
            REQUIRE(p.host.relay(action, &err));
            REQUIRE(p.host.sendFingerprint(fingerprintText(hostGame), &err));

            bool applied = false;
            std::string fp;
            for (int i = 0; i < 500 && (!applied || fp.empty()); ++i) {
                p.client.poll(p.ce);
                for (const Action& echoed : p.ce.actions) {
                    REQUIRE(apply(clientGame, echoed, &err));
                    applied = true;
                }
                if (!p.ce.fingerprint.empty()) fp = p.ce.fingerprint;
            }
            REQUIRE(applied);
            REQUIRE(!fp.empty());
            CHECK_EQ(fp, fingerprintText(clientGame));
        } else {
            // Client request: host validates and echoes it back.
            REQUIRE(p.client.request(action, &err));
            bool received = false;
            for (int i = 0; i < 500 && !received; ++i) {
                p.host.poll(p.he);
                if (!p.he.actions.empty()) {
                    REQUIRE(apply(hostGame, p.he.actions.back(), &err));
                    REQUIRE(p.host.relay(p.he.actions.back(), &err));
                    REQUIRE(p.host.sendFingerprint(fingerprintText(hostGame),
                                                   &err));
                    received = true;
                }
            }
            REQUIRE(received);

            bool applied = false;
            std::string fp;
            for (int i = 0; i < 500 && (!applied || fp.empty()); ++i) {
                p.client.poll(p.ce);
                for (const Action& echoed : p.ce.actions) {
                    REQUIRE(apply(clientGame, echoed, &err));
                    applied = true;
                }
                if (!p.ce.fingerprint.empty()) fp = p.ce.fingerprint;
            }
            REQUIRE(applied);
            REQUIRE(!fp.empty());
            CHECK_EQ(fp, fingerprintText(clientGame));
        }
        CHECK_EQ(fingerprintText(hostGame), fingerprintText(clientGame));
    }

    CHECK(steps > 100);
    CHECK_EQ(fingerprintText(hostGame), fingerprintText(clientGame));
}

TEST_CASE("session rejects join and start in the wrong role") {
    Session host;
    Session client;
    std::string err;

    CHECK(!host.start(1u, &err));
    CHECK(!host.request(actionOf(ActionKind::DrawStock), &err));
    CHECK(!host.relay(actionOf(ActionKind::DrawStock), &err));
    CHECK(!host.sendFingerprint("0123456789abcdef", &err));
    CHECK(!client.start(1u, &err));
    CHECK(!client.relay(actionOf(ActionKind::DrawStock), &err));
    CHECK(!client.sendFingerprint("0123456789abcdef", &err));

    SessionEvents events;
    host.poll(events);
    client.poll(events);
    CHECK(!events.connected);
    CHECK(!events.started);
}
