// Machine-readable state dumps and card-accounting invariants.
#include "report.h"

#include "game.h"

#include "test_framework.hpp"
#include "test_helpers.h"

#include <string>

using namespace buraco;
using testutil::loadText;
using testutil::meldLine;
using testutil::pileLine;
using testutil::playerLine;
using testutil::saveHeader;

TEST_CASE("checkInvariants accepts a freshly dealt match") {
    Game g;
    g.newMatch("Ana", 123u);
    std::string err;
    CHECK(report::checkInvariants(g, err));
    CHECK(err.empty());
}

TEST_CASE("checkInvariants accepts a match in progress") {
    Game g;
    g.newMatch("Ana", 999u);
    for (int i = 0; i < 120 && !g.matchOver(); ++i) {
        if (g.phase() == Phase::HandOver) {
            g.continueAfterHand();
            continue;
        }
        g.autoStep();
        if (g.phase() == Phase::HandOver || g.phase() == Phase::MatchOver) continue;
        std::string err;
        CHECK(report::checkInvariants(g, err));
    }
}

TEST_CASE("checkInvariants reports a duplicated card") {
    Game g;
    g.newMatch("Ana", 5u);
    const Card extra = g.player(0).hand.front();
    g.player(0).hand.push_back(extra);
    std::string err;
    CHECK(!report::checkInvariants(g, err));
    CHECK(err.rfind("duplicated card", 0) == 0);
}

TEST_CASE("checkInvariants reports an invalid card id") {
    Game g;
    g.newMatch("Ana", 5u);
    Card bogus;
    bogus.id = 200;
    g.player(0).hand.push_back(bogus);
    std::string err;
    CHECK(!report::checkInvariants(g, err));
    CHECK_EQ(err, std::string("invalid card id in hand"));
}

TEST_CASE("checkInvariants reports a hole inside a meld") {
    Game g;
    const std::string text = saveHeader(int(Phase::Play)) + playerLine(0, {13}) +
                             "melds 0 1\n" +
                             meldLine(0, DIAMONDS, {{1, 1}, {3, 3}});
    REQUIRE(loadText(g, text, "report_hole.sav"));
    std::string err;
    CHECK(!report::checkInvariants(g, err));
    CHECK_EQ(err, std::string("meld with a hole"));
}

TEST_CASE("checkInvariants reports an empty meld entry") {
    Game g;
    g.newMatch("Ana", 5u);
    g.player(1).melds.push_back(Meld{});
    std::string err;
    CHECK(!report::checkInvariants(g, err));
    CHECK_EQ(err, std::string("empty meld in list"));
}

TEST_CASE("checkInvariants reports a missing card") {
    Game g;
    g.newMatch("Ana", 5u);
    REQUIRE(!g.player(0).hand.empty());
    // Remove one card without putting it anywhere else.
    g.player(0).hand.pop_back();
    std::string err;
    CHECK(!report::checkInvariants(g, err));
    CHECK(err.find("card count 107 != 108") != std::string::npos);
}

TEST_CASE("checkInvariants accepts a legal meld") {
    Game g;
    std::vector<uint8_t> rest;
    for (int id = 1; id <= DECK_SIZE; ++id)
        if (id != 1 && id != 2 && id != 3 && id != 13) rest.push_back(uint8_t(id));
    const std::string text = saveHeader(int(Phase::Play)) + playerLine(0, {13}) +
                             "melds 0 1\n" +
                             meldLine(0, DIAMONDS, {{1, 1}, {2, 2}, {3, 3}}) +
                             pileLine("monte", rest);
    REQUIRE(loadText(g, text, "report_meld.sav"));
    std::string err;
    CHECK(report::checkInvariants(g, err));
}

TEST_CASE("stateLine describes the visible state") {
    Game g;
    g.newMatch("Ana", 123u);
    const std::string line = report::stateLine(g);
    CHECK(line.rfind("# state phase=", 0) == 0);
    CHECK(line.find("cur=0") != std::string::npos);
    CHECK(line.find("hand=") != std::string::npos);
    CHECK(line.find("ai=") != std::string::npos);
    CHECK(line.find("monte=") != std::string::npos);
    CHECK(line.find("pts ") != std::string::npos);
    CHECK(line.find("stats 0/0/0") != std::string::npos);
    CHECK_EQ(line.back(), '\n');
}

TEST_CASE("stateLine lists melds with their cards and value") {
    Game g;
    const std::string text = saveHeader(int(Phase::Play)) + playerLine(0, {13}) +
                             "melds 0 1\n" +
                             meldLine(0, DIAMONDS, {{1, 1}, {2, 2}, {3, 3}});
    REQUIRE(loadText(g, text, "report_line.sav"));
    const std::string line = report::stateLine(g);
    CHECK(line.find("p0 melds: s1 n3 v30[1,2,3,]") != std::string::npos);
}

TEST_CASE("strictLine starts with stateLine and adds physical ids") {
    Game g;
    g.newMatch("Ana", 321u);
    const std::string state = report::stateLine(g);
    const std::string strict = report::strictLine(g);
    CHECK(strict.rfind(state, 0) == 0);
    CHECK(strict.find("H0:") != std::string::npos);
    CHECK(strict.find("H1:") != std::string::npos);
}
