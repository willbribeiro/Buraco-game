// Game state machine tests.  Private state is reached through save files,
// which is also how the real game restores a match; the public API is enough
// for every assertion below.
#include "game.h"

#include "test_framework.hpp"
#include "test_helpers.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

using namespace buraco;
using testutil::loadText;
using testutil::meldLine;
using testutil::pileLine;
using testutil::playerLine;
using testutil::saveHeader;
using testutil::snapshot;

namespace {

std::string player0Hand(std::vector<uint8_t> hand, bool tookMorto = true) {
    return playerLine(0, hand, tookMorto);
}

void advance(Game& g, int steps) {
    for (int i = 0; i < steps; ++i) {
        if (g.matchOver()) break;
        if (g.phase() == Phase::HandOver) {
            g.continueAfterHand();
            continue;
        }
        g.autoStep();
    }
}

TEST_CASE("newMatch deals a fresh match") {
    Game g;
    CHECK_EQ(g.player(0).name, std::string("Voce"));
    CHECK_EQ(g.player(1).name, std::string("Eu"));
    CHECK_EQ(g.handNumber(), 0);

    g.newMatch("Ana", 42u);
    CHECK_EQ(g.player(0).name, std::string("Ana"));
    CHECK_EQ(g.player(0).hand.size(), size_t(HAND_SIZE));
    CHECK_EQ(g.player(1).hand.size(), size_t(HAND_SIZE));
    CHECK_EQ(g.morto().size(), size_t(MORTO_SIZE));
    CHECK_EQ(g.monte().size(), size_t(DECK_SIZE - 2 * HAND_SIZE - MORTO_SIZE));
    CHECK_EQ(g.handNumber(), 1);
    CHECK_EQ(g.current(), 0);
    CHECK_EQ(int(g.phase()), int(Phase::Draw));
    CHECK_EQ(g.handsPlayed(), 0);
    CHECK_EQ(g.targetScore(), 3000);
}

TEST_CASE("newMatch is deterministic for the same seed") {
    Game a;
    Game b;
    a.newMatch("A", 1234u);
    b.newMatch("B", 1234u);
    REQUIRE(a.player(0).hand.size() == b.player(0).hand.size());
    for (size_t i = 0; i < a.player(0).hand.size(); ++i)
        CHECK_EQ(int(a.player(0).hand[i].id), int(b.player(0).hand[i].id));
    CHECK_EQ(a.monte().size(), b.monte().size());
    for (size_t i = 0; i < a.monte().size(); ++i)
        CHECK_EQ(int(a.monte()[i].id), int(b.monte()[i].id));
    CHECK_EQ(a.rng(), b.rng());
}

TEST_CASE("different seeds deal different cards") {
    Game a;
    Game b;
    a.newMatch("A", 1u);
    b.newMatch("B", 2u);
    bool differs = false;
    for (size_t i = 0; i < a.player(0).hand.size() && !differs; ++i)
        differs = a.player(0).hand[i].id != b.player(0).hand[i].id;
    CHECK(differs);
}

TEST_CASE("newHand alternates the starting player") {
    Game g;
    g.newMatch("A", 7u);
    CHECK_EQ(g.current(), 0);

    g.newHand();
    CHECK_EQ(g.handNumber(), 2);
    CHECK_EQ(g.current(), 1);

    g.newHand();
    CHECK_EQ(g.handNumber(), 3);
    CHECK_EQ(g.current(), 0);
}

TEST_CASE("all 108 cards are dealt exactly once") {
    Game g;
    g.newMatch("A", 99u);
    std::vector<int> seen(DECK_SIZE + 1, 0);
    auto count = [&](const Card& c) {
        REQUIRE(c.valid());
        REQUIRE(c.id <= DECK_SIZE);
        ++seen[c.id];
    };
    for (int seat = 0; seat < 2; ++seat) {
        for (const Card& c : g.player(seat).hand) count(c);
        for (const Meld& m : g.player(seat).melds)
            for (const Card& c : m.cards()) count(c);
    }
    for (const Card& c : g.morto()) count(c);
    for (const Card& c : g.monte()) count(c);
    for (const Card& c : g.discardPile()) count(c);
    for (int id = 1; id <= DECK_SIZE; ++id) CHECK_EQ(seen[id], 1);
}

TEST_CASE("draw from the monte") {
    Game g;
    g.newMatch("A", 123u);
    const size_t before = g.player(0).hand.size();
    const size_t monte = g.monte().size();

    REQUIRE(g.draw(false));
    CHECK_EQ(g.player(0).hand.size(), before + 1);
    CHECK_EQ(g.monte().size(), monte - 1);
    CHECK_EQ(int(g.phase()), int(Phase::Play));
    CHECK_EQ(int(g.lastDrawn().id), int(g.player(0).hand.back().id));
}

TEST_CASE("draw handles an empty monte and an empty table") {
    Game g;
    CHECK(!g.draw(true));  // still in Draw but the pile is empty
    CHECK_EQ(int(g.phase()), int(Phase::Draw));

    std::string text = saveHeader(0) + player0Hand({}) + pileLine("mesa", {});
    REQUIRE(loadText(g, text, "empty_monte.sav"));
    CHECK(!g.draw(true));
    CHECK(!g.draw(false));
    CHECK_EQ(g.player(0).hand.size(), size_t(0));
    CHECK_EQ(int(g.phase()), int(Phase::Play));
    CHECK_EQ(g.message(), std::string("O monte acabou."));
}

TEST_CASE("buying the table takes the whole pile") {
    Game g;
    std::string text = saveHeader(0) + player0Hand({}) + pileLine("mesa", {5, 6, 7});
    REQUIRE(loadText(g, text, "buy_table.sav"));
    CHECK(g.canTakeDiscard());
    REQUIRE(g.draw(true));
    CHECK_EQ(g.player(0).hand.size(), size_t(3));
    CHECK_EQ(int(g.player(0).hand[0].id), 5);
    CHECK_EQ(int(g.player(0).hand[2].id), 7);
    CHECK(g.discardPile().empty());
    CHECK_EQ(int(g.lastDrawn().id), 7);
}

TEST_CASE("layMeld validates phase and selection") {
    Game g;
    g.newMatch("A", 123u);
    CHECK(!g.layMeld({makeCard(1), makeCard(2), makeCard(3)}).empty());

    std::string text = saveHeader(int(Phase::Play)) + player0Hand({1, 2, 3, 13});
    REQUIRE(loadText(g, text, "lay_phase.sav"));
    CHECK(!g.layMeld({}).empty());
    CHECK(!g.layMeld({makeCard(1)}).empty());  // no existing meld to grow
    CHECK_EQ(g.player(0).hand.size(), size_t(4));
}

TEST_CASE("layMeld creates a meld and keeps the hand") {
    Game g;
    std::string text = saveHeader(int(Phase::Play)) + player0Hand({1, 2, 3, 13});
    REQUIRE(loadText(g, text, "lay_new.sav"));

    CHECK(g.layMeld({makeCard(1), makeCard(2), makeCard(3)}).empty());
    REQUIRE(g.player(0).melds.size() == 1);
    const Meld& m = g.player(0).melds[0];
    CHECK_EQ(int(m.suit), int(DIAMONDS));
    CHECK_EQ(m.size(), 3);
    CHECK_EQ(m.value(), 30);
    CHECK_EQ(g.player(0).hand.size(), size_t(1));
    CHECK_EQ(int(g.player(0).hand[0].id), 13);
    CHECK_EQ(int(g.phase()), int(Phase::Play));
}

TEST_CASE("layMeld grows an existing meld with a single card") {
    Game g;
    std::string text = saveHeader(int(Phase::Play)) + playerLine(0, {4, 5}) + "melds 0 1\n" +
                       meldLine(0, DIAMONDS, {{1, 1}, {2, 2}, {3, 3}});
    REQUIRE(loadText(g, text, "lay_grow.sav"));
    REQUIRE(g.player(0).melds.size() == 1);

    CHECK(g.layMeld({makeCard(4)}).empty());
    CHECK_EQ(g.player(0).melds[0].size(), 4);
    CHECK_EQ(g.player(0).hand.size(), size_t(1));
    CHECK_EQ(int(g.player(0).hand[0].id), 5);
}

TEST_CASE("layMeld honours an explicit target") {
    Game g;
    std::string text = saveHeader(int(Phase::Play)) + playerLine(0, {4, 5, 13}) + "melds 0 1\n" +
                       meldLine(0, DIAMONDS, {{1, 1}, {2, 2}, {3, 3}});
    REQUIRE(loadText(g, text, "lay_target.sav"));

    CHECK(!g.layMeld({makeCard(4)}, 99).empty());
    CHECK(g.layMeld({makeCard(4)}, 0).empty());
    CHECK_EQ(g.player(0).melds[0].size(), 4);
}

TEST_CASE("layMeld kNewMeld and the fifteen-meld limit") {
    Game g;
    std::string text = saveHeader(int(Phase::Play)) + player0Hand({1, 2, 3, 13});
    REQUIRE(loadText(g, text, "lay_force.sav"));
    CHECK(g.layMeld({makeCard(1), makeCard(2), makeCard(3)}, Game::kNewMeld).empty());
    REQUIRE(g.player(0).melds.size() == 1);

    std::ostringstream full;
    full << saveHeader(int(Phase::Play)) << player0Hand({1, 2, 3, 13}) << "melds 0 15\n";
    for (int i = 0; i < 15; ++i)
        full << meldLine(0, DIAMONDS, {{1, 1}, {2, 2}, {3, 3}});
    REQUIRE(loadText(g, full.str(), "lay_full.sav"));
    REQUIRE(g.player(0).melds.size() == size_t(15));
    CHECK_EQ(g.layMeld({makeCard(1), makeCard(2), makeCard(3)}, Game::kNewMeld),
             std::string("Nao cabem mais jogos na mesa."));
}

TEST_CASE("layOptions reports growth targets and new-meld room") {
    Game g;
    std::string text = saveHeader(int(Phase::Play)) + playerLine(0, {4, 5, 13, 14}) + "melds 0 1\n" +
                       meldLine(0, DIAMONDS, {{1, 1}, {2, 2}, {3, 3}});
    REQUIRE(loadText(g, text, "lay_options.sav"));

    Game::LayOptions grow = g.layOptions({makeCard(4)});
    CHECK(!grow.isMeld);
    CHECK(!grow.canCreate);
    REQUIRE(grow.targets.size() == 1);
    CHECK_EQ(grow.targets[0], 0);

    Game::LayOptions fresh = g.layOptions({makeCard(1), makeCard(2), makeCard(3)});
    CHECK(fresh.isMeld);
    CHECK(fresh.canCreate);

    Game::LayOptions none = g.layOptions({});
    CHECK(!none.isMeld);
    CHECK(!none.canCreate);
    CHECK(none.targets.empty());
}

TEST_CASE("discarding moves the card to the table and passes the turn") {
    Game g;
    std::string text = saveHeader(int(Phase::Discard)) + player0Hand({5, 6});
    REQUIRE(loadText(g, text, "discard.sav"));

    REQUIRE(g.discardCard(makeCard(5)));
    CHECK_EQ(g.player(0).hand.size(), size_t(1));
    REQUIRE(g.discardPile().size() == 1);
    CHECK_EQ(int(g.discardPile()[0].id), 5);
    CHECK_EQ(g.current(), 1);
    CHECK_EQ(int(g.phase()), int(Phase::Draw));

    CHECK(!g.discardCard(makeCard(5)));  // wrong phase
}

TEST_CASE("discarding the last card takes the morto") {
    Game g;
    std::string text = saveHeader(int(Phase::Discard)) + player0Hand({5}, false) + pileLine("morto", {7, 8});
    REQUIRE(loadText(g, text, "take_morto.sav"));

    CHECK(g.discardCard(makeCard(5)));
    CHECK_EQ(g.player(0).hand.size(), size_t(2));
    CHECK(g.player(0).tookMorto);
    CHECK_EQ(g.mortoRemaining(), 0);
    CHECK_EQ(g.current(), 1);
    CHECK_EQ(int(g.phase()), int(Phase::Draw));
}

TEST_CASE("discarding the last card with the morto already taken scores the hand") {
    Game g;
    std::string text = saveHeader(int(Phase::Discard)) + player0Hand({5}, true) + playerLine(1, {}, true);
    REQUIRE(loadText(g, text, "batida_discard.sav"));

    CHECK(g.discardCard(makeCard(5)));
    CHECK_EQ(int(g.phase()), int(Phase::HandOver));
    CHECK_EQ(int(g.prompt()), int(Prompt::NewHand));
    CHECK_EQ(g.lastWentOutSeat(), 0);
    CHECK_EQ(g.player(0).lastHandPoints, 100);
    CHECK_EQ(g.player(1).lastHandPoints, 0);
    CHECK_EQ(g.player(0).total, 100);
    CHECK_EQ(g.handsPlayed(), 1);
    CHECK_EQ(g.wins(0), 1);
    CHECK_EQ(g.wins(1), 0);
    CHECK(g.player(0).wentOut);
}

TEST_CASE("laying the last card scores the melds plus the batida") {
    Game g;
    std::string text = saveHeader(int(Phase::Play)) + player0Hand({1, 2, 3}, false) + playerLine(1, {}, true);
    REQUIRE(loadText(g, text, "batida_meld.sav"));

    CHECK(g.layMeld({makeCard(1), makeCard(2), makeCard(3)}).empty());
    CHECK_EQ(int(g.phase()), int(Phase::HandOver));
    CHECK_EQ(g.lastWentOutSeat(), 0);
    CHECK_EQ(g.player(0).lastHandPoints, 130);  // 30 meld + 100 batida
    CHECK_EQ(g.player(0).total, 130);
}

TEST_CASE("laying the last card takes the morto before scoring") {
    Game g;
    std::string text = saveHeader(int(Phase::Play)) + player0Hand({1, 2, 3}, false) +
                       playerLine(1, {}, true) + pileLine("morto", {7, 8, 9});
    REQUIRE(loadText(g, text, "morto_after_meld.sav"));

    CHECK(g.layMeld({makeCard(1), makeCard(2), makeCard(3)}).empty());
    CHECK(g.player(0).tookMorto);
    CHECK_EQ(g.mortoRemaining(), 0);
    CHECK_EQ(g.player(0).hand.size(), size_t(3));
    CHECK_EQ(int(g.player(0).hand[0].id), 9);  // morto is taken from its back
    CHECK_EQ(int(g.phase()), int(Phase::Discard));
    CHECK_EQ(g.lastWentOutSeat(), -1);
    CHECK_EQ(g.player(0).melds.size(), size_t(1));
}

TEST_CASE("endHand charges cards in hand and the missing morto") {
    Game g;
    std::string text = saveHeader(int(Phase::Discard)) + player0Hand({5}, true) + playerLine(1, {53}, false);
    REQUIRE(loadText(g, text, "score_penalty.sav"));

    CHECK(g.discardCard(makeCard(5)));
    CHECK_EQ(g.player(0).lastHandPoints, 100);  // went out, empty hand
    CHECK_EQ(g.player(1).lastHandPoints, -120); // joker in hand (20) + morto (100)
}

TEST_CASE("endHand includes canastra bonuses") {
    std::string clean = saveHeader(int(Phase::Discard)) + player0Hand({14}) + playerLine(1, {}, true) +
                        "melds 0 1\n" +
                        meldLine(0, DIAMONDS, {{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 6}, {7, 7}});
    Game g;
    REQUIRE(loadText(g, clean, "score_clean.sav"));
    CHECK(g.discardCard(makeCard(14)));
    CHECK_EQ(g.player(0).lastHandPoints, 350);  // 250 clean canastra + 100 batida

    std::ostringstream real;
    real << saveHeader(int(Phase::Discard)) << player0Hand({14}) << playerLine(1, {}, true) << "melds 0 1\n"
         << meldLine(0, DIAMONDS,
                     {{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 6}, {7, 7},
                      {8, 8}, {9, 9}, {10, 10}, {11, 11}, {12, 12}, {13, 13}});
    Game h;
    REQUIRE(loadText(h, real.str(), "score_real.sav"));
    CHECK(h.discardCard(makeCard(14)));
    CHECK_EQ(h.player(0).lastHandPoints, 710);  // 610 real canastra + 100 batida
}

TEST_CASE("AI update respects the delay and only runs for seat 1") {
    Game g;
    std::string text = saveHeader(0, 1) + player0Hand({}) + playerLine(1, {}) +
                       pileLine("monte", {5, 6});
    REQUIRE(loadText(g, text, "ai_delay.sav"));
    g.update(0.5);
    CHECK_EQ(g.player(1).hand.size(), size_t(0));
    g.update(0.3);
    CHECK_EQ(g.player(1).hand.size(), size_t(1));
    CHECK_EQ(int(g.phase()), int(Phase::Play));
}

TEST_CASE("autoStep plays one phase at a time") {
    Game g;
    std::string text = saveHeader(0, 1) + player0Hand({}) + playerLine(1, {5, 6}) +
                       pileLine("monte", {7, 8});
    REQUIRE(loadText(g, text, "auto_step.sav"));
    CHECK_EQ(int(g.phase()), int(Phase::Draw));

    g.autoStep();
    CHECK_EQ(int(g.phase()), int(Phase::Play));
    CHECK_EQ(g.player(1).hand.size(), size_t(3));

    g.autoStep();
    CHECK_EQ(int(g.phase()), int(Phase::Discard));

    g.autoStep();
    CHECK_EQ(g.current(), 0);
    CHECK_EQ(int(g.phase()), int(Phase::Draw));
}

TEST_CASE("requestDiscard only works from the play phase") {
    Game g;
    std::string text = saveHeader(int(Phase::Play)) + player0Hand({5});
    REQUIRE(loadText(g, text, "request_discard.sav"));
    g.requestDiscard();
    CHECK_EQ(int(g.phase()), int(Phase::Discard));

    std::string draw = saveHeader(0) + player0Hand({5});
    REQUIRE(loadText(g, draw, "request_discard_draw.sav"));
    g.requestDiscard();
    CHECK_EQ(int(g.phase()), int(Phase::Draw));
}

TEST_CASE("continueAfterHand deals again below the target score") {
    Game g;
    std::string text = saveHeader(int(Phase::HandOver)) + player0Hand({}, true) + playerLine(1, {}, true, 0, 2999);
    REQUIRE(loadText(g, text, "continue_below.sav"));
    g.continueAfterHand();
    CHECK_EQ(g.handNumber(), 2);
    CHECK_EQ(int(g.phase()), int(Phase::Draw));
    CHECK_EQ(g.player(1).total, 2999);
}

TEST_CASE("continueAfterHand ends the match at the target score") {
    Game g;
    std::string text = saveHeader(int(Phase::HandOver)) + player0Hand({}, true) + playerLine(1, {}, true, 0, 3000);
    REQUIRE(loadText(g, text, "continue_target.sav"));
    g.continueAfterHand();
    CHECK(g.matchOver());
    CHECK_EQ(int(g.phase()), int(Phase::MatchOver));
    CHECK_EQ(g.message(), std::string("A partida acabou !!!"));
}

TEST_CASE("save/load round-trips the complete state") {
    Game g;
    g.newMatch("Ana", 123u);
    advance(g, 350);
    const std::string before = snapshot(g);
    const std::string path = testutil::tempSavePath("roundtrip.sav");

    REQUIRE(g.saveTo(path));
    Game restored;
    REQUIRE(restored.loadFrom(path));
    CHECK_EQ(snapshot(restored), before);

    advance(g, 150);
    advance(restored, 150);
    CHECK_EQ(snapshot(restored), snapshot(g));
}

TEST_CASE("loadFrom rejects corrupt files without touching the live game") {
    Game g;
    g.newMatch("Ana", 5u);
    advance(g, 40);
    const std::string before = snapshot(g);

    CHECK(!g.loadFrom(testutil::tempSavePath("does_not_exist.sav")));
    CHECK_EQ(snapshot(g), before);

    const std::string bad = testutil::tempSavePath("bad.sav");
    REQUIRE(testutil::writeText(bad, "not a save\n"));
    CHECK(!g.loadFrom(bad));
    CHECK_EQ(snapshot(g), before);

    std::string unknown = saveHeader(int(Phase::Play)) + player0Hand({1, 2, 3}) + "nonsense 1 2 3\n";
    REQUIRE(testutil::writeText(bad, unknown));
    CHECK(!g.loadFrom(bad));
    CHECK_EQ(snapshot(g), before);
}

TEST_CASE("loadFrom reads old saves without the stats line") {
    Game g;
    std::ostringstream out;
    out << "BURACO-SAVE 1\nhand 4\ncurrent 0\nphase 2\n";
    out << playerLine(0, {1, 2, 3});
    REQUIRE(loadText(g, out.str(), "old_save.sav"));
    CHECK_EQ(g.handNumber(), 4);
    CHECK_EQ(g.handsPlayed(), 3);
    CHECK_EQ(g.wins(0), 0);
    CHECK_EQ(g.wins(1), 0);
}

TEST_CASE("loadFrom validates ranges and counts") {
    Game g;
    std::string badPhase = "BURACO-SAVE 1\nhand 1\nphase 9\n";
    CHECK(!loadText(g, badPhase, "bad_phase.sav"));
    std::string badSeat = "BURACO-SAVE 1\nhand 1\ncurrent 2\n";
    CHECK(!loadText(g, badSeat, "bad_seat.sav"));
    std::string badCard = "BURACO-SAVE 1\nhand 1\nplayer 0 1 0 0 0 1 200\n";
    CHECK(!loadText(g, badCard, "bad_card.sav"));
    std::string badMeldCount = "BURACO-SAVE 1\nhand 1\nmelds 0 99\n";
    CHECK(!loadText(g, badMeldCount, "bad_melds.sav"));
    std::string missingMeld = "BURACO-SAVE 1\nhand 1\nmelds 0 1\n";
    CHECK(!loadText(g, missingMeld, "missing_meld.sav"));
}

TEST_CASE("the prompt enum is persisted") {
    Game g;
    std::string text = saveHeader(int(Phase::HandOver)) + "prompt 2\n" + player0Hand({}, true);
    REQUIRE(loadText(g, text, "prompt.sav"));
    CHECK_EQ(int(g.prompt()), int(Prompt::NewHand));
}

TEST_CASE("controllers start as human vs AI and can be changed") {
    Game g;
    CHECK(g.isHuman(0));
    CHECK(g.isAi(1));
    CHECK(!g.isHotseat());
    CHECK_EQ(int(g.controller(0)), int(Controller::Human));
    CHECK_EQ(int(g.controller(1)), int(Controller::Ai));

    g.setController(1, Controller::Human);
    CHECK(g.isHotseat());
    CHECK(g.isHuman(0));
    CHECK(g.isHuman(1));

    g.setController(0, Controller::Ai);
    CHECK(g.isAi(0));
    CHECK(!g.isHotseat());

    g.setController(1, Controller::Remote);
    CHECK_EQ(int(g.controller(1)), int(Controller::Remote));
    CHECK(!g.isHuman(1));
    CHECK(!g.isAi(1));
}

TEST_CASE("update only plays an AI seat") {
    Game g;
    g.newMatch("Ana", 7u);
    CHECK_EQ(g.current(), 0);
    const size_t dealt = g.player(0).hand.size();

    // Seat 0 is human: time passing must not take a card for it.
    g.update(5.0);
    CHECK_EQ(int(g.phase()), int(Phase::Draw));
    CHECK_EQ(g.player(0).hand.size(), dealt);

    // Once the seat is handed to the AI, the same tick plays its draw phase.
    g.setController(0, Controller::Ai);
    g.update(5.0);
    CHECK_EQ(int(g.phase()), int(Phase::Play));
    CHECK_EQ(g.player(0).hand.size(), dealt + 1);
}

TEST_CASE("update pauses in a hot-seat match") {
    Game g;
    g.newMatch("Ana", 9u, "Bia");
    g.setController(0, Controller::Human);
    g.setController(1, Controller::Human);
    const size_t hand0 = g.player(0).hand.size();
    const size_t hand1 = g.player(1).hand.size();

    g.update(10.0);
    CHECK_EQ(int(g.phase()), int(Phase::Draw));
    CHECK_EQ(g.player(0).hand.size(), hand0);
    CHECK_EQ(g.player(1).hand.size(), hand1);
}

TEST_CASE("save/load keeps the per-seat controllers") {
    Game g;
    g.newMatch("Ana", 5u, "Bia");
    g.setController(0, Controller::Human);
    g.setController(1, Controller::Human);
    const std::string path = testutil::tempSavePath("hotseat_ctrl.sav");
    REQUIRE(g.saveTo(path));

    Game h;
    REQUIRE(h.loadFrom(path));
    CHECK(h.isHotseat());
    CHECK_EQ(h.player(0).name, std::string("Ana"));
    CHECK_EQ(h.player(1).name, std::string("Bia"));
}

TEST_CASE("saves without a ctrl line still load as solo") {
    Game g;
    g.setController(0, Controller::Human);
    g.setController(1, Controller::Human);
    const std::string text = saveHeader() + player0Hand({1, 2, 3});
    REQUIRE(loadText(g, text, "old_ctrl.sav"));
    CHECK(g.isHuman(0));
    CHECK(g.isAi(1));
    CHECK(!g.isHotseat());
}

TEST_CASE("loadFrom rejects invalid controller values") {
    Game g;
    const std::string before = snapshot(g);
    const std::string negative = saveHeader() + "ctrl -1 1\n";
    CHECK(!loadText(g, negative, "bad_ctrl_negative.sav"));
    const std::string high = saveHeader() + "ctrl 0 9\n";
    CHECK(!loadText(g, high, "bad_ctrl_high.sav"));
    CHECK_EQ(snapshot(g), before);
}

}  // namespace
