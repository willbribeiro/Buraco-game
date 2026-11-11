// Transport, protocol and lockstep tests for the network foundation.
#include <thread>

#include "net.h"
#include "net_protocol.h"
#include "report.h"
#include "test_framework.hpp"

using namespace buraco;
using namespace buraco::net;

namespace {

std::string payloadOf(const Action& action) { return makeAction(action); }

Action roundTrip(const Action& action) {
    Action parsed;
    std::string err;
    const std::string wire = makeAction(action);
    REQUIRE(parseAction(wire, parsed, &err));
    CHECK(err.empty());
    return parsed;
}

}  // namespace

TEST_CASE("net frames messages over a loopback connection") {
    std::string err;
    Socket server;
    REQUIRE(server.listen(0, &err));
    const std::uint16_t port = server.localPort(&err);
    REQUIRE(port != 0);

    std::string workerErr;
    std::string echoed;
    IoStatus workerStatus = IoStatus::Error;
    std::thread worker([&] {
        auto peer = server.accept(3000, &workerErr);
        if (!peer) return;
        std::string first, second;
        if (recvMessage(*peer, first, &workerErr) != IoStatus::Ok) return;
        if (recvMessage(*peer, second, &workerErr) != IoStatus::Ok) return;
        echoed = first + "|" + second;
        workerStatus = sendMessage(*peer, echoed, &workerErr) ? IoStatus::Ok
                                                              : IoStatus::Error;
    });

    Socket client;
    REQUIRE(client.connect("127.0.0.1", port, 3000, &err));
    client.setRecvTimeout(3000);
    REQUIRE(sendMessage(client, "hello", &err));
    REQUIRE(sendMessage(client, "world", &err));
    std::string reply;
    const IoStatus status = recvMessage(client, reply, &err);
    worker.join();

    CHECK(workerStatus == IoStatus::Ok);
    CHECK(status == IoStatus::Ok);
    CHECK_EQ(reply, std::string("hello|world"));
    CHECK_EQ(echoed, reply);
}

TEST_CASE("net receive times out when the peer stays silent") {
    std::string err;
    Socket server;
    REQUIRE(server.listen(0, &err));
    const std::uint16_t port = server.localPort(&err);
    REQUIRE(port != 0);

    Socket client;
    REQUIRE(client.connect("127.0.0.1", port, 3000, &err));
    client.setRecvTimeout(100);
    std::string message;
    err.clear();
    CHECK(recvMessage(client, message, &err) == IoStatus::Timeout);
    CHECK(err.find("timeout") != std::string::npos);
}

TEST_CASE("net rejects an oversized frame") {
    std::string err;
    Socket server;
    REQUIRE(server.listen(0, &err));
    const std::uint16_t port = server.localPort(&err);
    REQUIRE(port != 0);

    IoStatus workerStatus = IoStatus::Ok;
    std::string workerErr;
    std::thread worker([&] {
        auto peer = server.accept(3000, &workerErr);
        if (!peer) return;
        std::string message;
        workerStatus = recvMessage(*peer, message, &workerErr);
    });

    Socket client;
    REQUIRE(client.connect("127.0.0.1", port, 3000, &err));
    const unsigned char header[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    REQUIRE(client.sendAll(header, sizeof header, &err) == IoStatus::Ok);
    worker.join();

    CHECK(workerStatus == IoStatus::Error);
    CHECK(workerErr.find("frame too large") != std::string::npos);
}

TEST_CASE("net detects a graceful close") {
    std::string err;
    Socket server;
    REQUIRE(server.listen(0, &err));
    const std::uint16_t port = server.localPort(&err);
    REQUIRE(port != 0);

    IoStatus workerStatus = IoStatus::Error;
    std::string workerErr;
    std::thread worker([&] {
        auto peer = server.accept(3000, &workerErr);
        if (!peer) return;
        std::string message;
        workerStatus = recvMessage(*peer, message, &workerErr);
    });

    Socket client;
    REQUIRE(client.connect("127.0.0.1", port, 3000, &err));
    client.setRecvTimeout(3000);
    REQUIRE(sendMessage(client, "bye", &err));
    std::string message;
    const IoStatus status = recvMessage(client, message, &err);
    worker.join();

    CHECK(workerStatus == IoStatus::Ok);
    CHECK(status == IoStatus::Closed);
}

TEST_CASE("hello carries the name and checks the version") {
    std::string name;
    std::string err;
    REQUIRE(parseHello(makeHello("Ana"), name, &err));
    CHECK_EQ(name, std::string("Ana"));

    // Long names are clipped on the wire and rejected by a strict parser.
    const std::string longName(80, 'a');
    REQUIRE(parseHello(makeHello(longName), name, &err));
    CHECK_EQ(name.size(), kMaxNameLength);

    name.clear();
    CHECK(!parseHello("BURACO-NET 2 HELLO Ana", name, &err));
    CHECK(!parseHello("BURACO-NET 1 HELLO Ana\nx", name, &err));
    CHECK(!parseHello("BURACO-X 1 HELLO Ana", name, &err));
    CHECK(!parseHello("BURACO-NET 1 BYE Ana", name, &err));
}

TEST_CASE("action messages round-trip") {
    Action action;
    action.kind = ActionKind::DrawStock;
    Action parsed = roundTrip(action);
    CHECK(parsed.kind == ActionKind::DrawStock);

    action.kind = ActionKind::DrawTable;
    parsed = roundTrip(action);
    CHECK(parsed.kind == ActionKind::DrawTable);

    action = Action{};
    action.kind = ActionKind::LayMeld;
    action.target = Game::kNewMeld;
    action.cards = {makeCard(1), makeCard(2), makeCard(15)};
    parsed = roundTrip(action);
    CHECK(parsed.kind == ActionKind::LayMeld);
    CHECK_EQ(parsed.target, int(Game::kNewMeld));
    REQUIRE(parsed.cards.size() == 3);
    CHECK_EQ(int(parsed.cards[0].id), 1);
    CHECK_EQ(int(parsed.cards[2].id), 15);

    action = Action{};
    action.kind = ActionKind::Discard;
    action.card = makeCard(42);
    parsed = roundTrip(action);
    CHECK(parsed.kind == ActionKind::Discard);
    CHECK_EQ(int(parsed.card.id), 42);

    action = Action{};
    action.kind = ActionKind::RequestDiscard;
    parsed = roundTrip(action);
    CHECK(parsed.kind == ActionKind::RequestDiscard);

    action = Action{};
    action.kind = ActionKind::PromptYes;
    parsed = roundTrip(action);
    CHECK(parsed.kind == ActionKind::PromptYes);

    action = Action{};
    action.kind = ActionKind::PromptNo;
    parsed = roundTrip(action);
    CHECK(parsed.kind == ActionKind::PromptNo);

    action = Action{};
    action.kind = ActionKind::Continue;
    parsed = roundTrip(action);
    CHECK(parsed.kind == ActionKind::Continue);
}

TEST_CASE("malformed action messages are rejected") {
    Action action;
    std::string err;
    CHECK(!parseAction("", action, &err));
    CHECK(!parseAction("BURACO-NET 2 ACT draw stock", action, &err));
    CHECK(!parseAction("BURACO-NET 1 ACT draw banana", action, &err));
    CHECK(!parseAction("BURACO-NET 1 ACT draw stock extra", action, &err));
    CHECK(!parseAction("BURACO-NET 1 ACT discard 0", action, &err));
    CHECK(!parseAction("BURACO-NET 1 ACT discard 109", action, &err));
    CHECK(!parseAction("BURACO-NET 1 ACT lay -3 1 5", action, &err));
    CHECK(!parseAction("BURACO-NET 1 ACT lay -1 2 5", action, &err));
    CHECK(!parseAction("BURACO-NET 1 ACT prompt maybe", action, &err));
    CHECK(!parseAction("BURACO-NET 1 ACT jump", action, &err));
    CHECK(!parseAction("BURACO-NET 1 ACT continue\nx", action, &err));
}

TEST_CASE("remote controllers never act on their own") {
    Game g;
    g.newMatch("Ana", 7u, "Bia");
    g.setController(0, Controller::Remote);
    g.setController(1, Controller::Remote);
    CHECK(!g.isHuman(0));
    CHECK(!g.isAi(0));
    CHECK(!g.isHuman(1));
    CHECK(!g.isAi(1));

    const std::uint64_t before = fingerprint(g);
    g.update(10.0);
    g.update(10.0);
    CHECK_EQ(fingerprint(g), before);
    CHECK(g.phase() == Phase::Draw);
    CHECK_EQ(int(g.player(0).hand.size()), HAND_SIZE);
}

TEST_CASE("illegal remote actions leave the game untouched") {
    Game g;
    g.newMatch("Ana", 11u, "Bia");
    g.setController(0, Controller::Remote);
    g.setController(1, Controller::Remote);
    const std::uint64_t before = fingerprint(g);

    Action discard;
    discard.kind = ActionKind::Discard;
    discard.card = makeCard(1);
    std::string err;
    CHECK(!apply(g, discard, &err));
    CHECK_EQ(fingerprint(g), before);

    Action table;
    table.kind = ActionKind::DrawTable;
    CHECK(!apply(g, table, &err));
    CHECK_EQ(fingerprint(g), before);

    Action lay;
    lay.kind = ActionKind::LayMeld;
    lay.cards = {makeCard(1), makeCard(2), makeCard(3)};
    CHECK(!apply(g, lay, &err));
    CHECK_EQ(fingerprint(g), before);
}

TEST_CASE("the same action stream keeps two peers in lockstep") {
    Game host;
    Game peer;
    host.newMatch("Ana", 2024u, "Bia");
    peer.newMatch("Ana", 2024u, "Bia");
    for (int seat = 0; seat < 2; ++seat) {
        host.setController(seat, Controller::Remote);
        peer.setController(seat, Controller::Remote);
    }
    CHECK_EQ(fingerprintText(host), fingerprintText(peer));

    std::string err;
    int steps = 0;
    for (; steps < 400 && host.phase() != Phase::MatchOver; ++steps) {
        Action action;
        switch (host.phase()) {
            case Phase::Draw:
                if (!host.monte().empty())
                    action.kind = ActionKind::DrawStock;
                else if (host.canTakeDiscard())
                    action.kind = ActionKind::DrawTable;
                else
                    goto done;
                break;
            case Phase::Play:
                action.kind = ActionKind::RequestDiscard;
                break;
            case Phase::Discard:
                action.kind = ActionKind::Discard;
                action.card = host.player(host.current()).hand.front();
                break;
            case Phase::HandOver:
                action.kind = ActionKind::Continue;
                break;
            case Phase::MatchOver:
                goto done;
        }

        const std::string wire = payloadOf(action);
        Action parsed;
        REQUIRE(parseAction(wire, parsed, &err));
        REQUIRE(apply(host, parsed, &err));
        REQUIRE(apply(peer, parsed, &err));
        CHECK_EQ(fingerprintText(host), fingerprintText(peer));
    }
done:
    CHECK(steps > 100);
}
