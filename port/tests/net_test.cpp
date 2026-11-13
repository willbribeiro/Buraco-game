// Transport, protocol and lockstep tests for the network foundation.
#include <chrono>
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

TEST_CASE("seat, start, fingerprint and error messages round-trip") {
    std::string err;

    int seat = -1;
    REQUIRE(parseSeat(makeSeat(0), seat, &err));
    CHECK_EQ(seat, 0);
    REQUIRE(parseSeat(makeSeat(1), seat, &err));
    CHECK_EQ(seat, 1);
    CHECK(!parseSeat(makeSeat(2), seat, &err));
    CHECK(!parseSeat("BURACO-NET 1 SEAT -1", seat, &err));
    CHECK(!parseSeat("BURACO-NET 1 SEAT 1 extra", seat, &err));
    CHECK(!parseSeat("BURACO-NET 1 HELLO Ana", seat, &err));

    std::uint32_t seed = 0;
    REQUIRE(parseStart(makeStart(12345u), seed, &err));
    CHECK_EQ(seed, 12345u);
    REQUIRE(parseStart(makeStart(4294967295u), seed, &err));
    CHECK_EQ(seed, 4294967295u);
    CHECK(!parseStart("BURACO-NET 1 START -1", seed, &err));
    CHECK(!parseStart("BURACO-NET 1 START 4294967296", seed, &err));
    CHECK(!parseStart("BURACO-NET 1 START abc", seed, &err));
    CHECK(!parseStart("BURACO-NET 1 START 12 34", seed, &err));

    std::string text;
    REQUIRE(parseFingerprint(makeFingerprint("0123456789abcdef"), text, &err));
    CHECK_EQ(text, std::string("0123456789abcdef"));
    REQUIRE(parseFingerprint(makeFingerprint("0123456789ABCDEF"), text, &err));
    CHECK_EQ(text, std::string("0123456789abcdef"));
    CHECK(!parseFingerprint(makeFingerprint("0123456789abcde"), text, &err));
    CHECK(!parseFingerprint(makeFingerprint("0123456789abcdeg"), text, &err));
    CHECK(!parseFingerprint(makeFingerprint("0123456789abcdef0"), text, &err));

    REQUIRE(parseError(makeError("jogada recusada"), text, &err));
    CHECK_EQ(text, std::string("jogada recusada"));
    const std::string withNewlines = std::string("linha 1\n") + "linha 2\r";
    REQUIRE(parseError(makeError(withNewlines), text, &err));
    CHECK_EQ(text, std::string("linha 1linha 2"));
    CHECK(!parseError("BURACO-NET 1 ACT draw stock", text, &err));

    CHECK_EQ(tagOf(makeHello("Ana")), std::string("HELLO"));
    CHECK_EQ(tagOf(makeAction(Action{})), std::string("ACT"));
    CHECK_EQ(tagOf(makeSeat(1)), std::string("SEAT"));
    CHECK_EQ(tagOf(makeStart(1)), std::string("START"));
    CHECK_EQ(tagOf(makeFingerprint("0123456789abcdef")), std::string("FP"));
    CHECK_EQ(tagOf(makeError("x")), std::string("ERR"));
    CHECK_EQ(tagOf(""), std::string());
    CHECK_EQ(tagOf("BURACO-NET 2 HELLO Ana"), std::string());
    CHECK_EQ(tagOf("BURACO-NET 1"), std::string());
}

namespace {

std::string framed(const std::string& payload) {
    const std::uint32_t size = std::uint32_t(payload.size());
    std::string out;
    out += char((size >> 24) & 0xFF);
    out += char((size >> 16) & 0xFF);
    out += char((size >> 8) & 0xFF);
    out += char(size & 0xFF);
    out += payload;
    return out;
}

}  // namespace

TEST_CASE("frame reader reassembles split and coalesced messages") {
    std::string err;
    Socket server;
    REQUIRE(server.listen(0, &err));
    const std::uint16_t port = server.localPort(&err);
    REQUIRE(port != 0);

    Socket client;
    REQUIRE(client.connect("127.0.0.1", port, 3000, &err));
    auto accepted = server.accept(3000, &err);
    REQUIRE(accepted.has_value());
    Socket peer = std::move(*accepted);
    REQUIRE(peer.setNonBlocking(true));

    const std::string first = framed("hello world");
    const std::string second = framed("second");
    // Split the first frame after its header plus one payload byte.
    REQUIRE(client.sendAll(first.data(), 5, &err) == IoStatus::Ok);

    FrameReader reader;
    std::vector<std::string> messages;
    const IoStatus partial = reader.read(peer, messages, &err);
    CHECK(partial == IoStatus::Ok || partial == IoStatus::Timeout);
    CHECK(messages.empty());

    const std::string rest = first.substr(5) + second;
    REQUIRE(client.sendAll(rest.data(), rest.size(), &err) == IoStatus::Ok);
    std::vector<std::string> got;
    for (int attempt = 0; attempt < 100 && got.size() < 2; ++attempt) {
        std::vector<std::string> batch;
        reader.read(peer, batch, &err);
        got.insert(got.end(), batch.begin(), batch.end());
        if (got.size() < 2)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(got.size() == 2);
    CHECK_EQ(got[0], std::string("hello world"));
    CHECK_EQ(got[1], std::string("second"));
}

TEST_CASE("frame reader rejects an oversized header") {
    std::string err;
    Socket server;
    REQUIRE(server.listen(0, &err));
    const std::uint16_t port = server.localPort(&err);
    REQUIRE(port != 0);

    Socket client;
    REQUIRE(client.connect("127.0.0.1", port, 3000, &err));
    auto accepted = server.accept(3000, &err);
    REQUIRE(accepted.has_value());
    Socket peer = std::move(*accepted);
    REQUIRE(peer.setNonBlocking(true));

    const unsigned char header[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    REQUIRE(client.sendAll(header, sizeof header, &err) == IoStatus::Ok);
    FrameReader reader;
    std::vector<std::string> messages;
    CHECK(reader.read(peer, messages, &err) == IoStatus::Error);
    CHECK(err.find("frame too large") != std::string::npos);
    CHECK(!peer.valid());
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
