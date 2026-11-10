// Sound cues: event emission by the game and deterministic sample rendering.
#include "sound.h"

#include "cards.h"
#include "game.h"

#include "test_framework.hpp"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

using buraco::Game;
using buraco::Phase;
using buraco::makeCard;
using buraco::sound::Event;

namespace {

bool sameEvents(const std::vector<Event>& got, std::initializer_list<Event> want) {
    return got == std::vector<Event>(want);
}

bool anyNonZero(const std::vector<int16_t>& samples) {
    return std::any_of(samples.begin(), samples.end(),
                       [](int16_t v) { return v != 0; });
}

// Renders until the synth runs out of cues (with a generous guard).
std::vector<int16_t> renderAll(buraco::sound::Synth& synth, int chunk = 512) {
    std::vector<int16_t> out;
    std::vector<int16_t> buffer;
    buffer.resize(size_t(chunk));
    for (int guard = 0; guard < 200 && !synth.idle(); ++guard) {
        synth.render(buffer.data(), chunk);
        out.insert(out.end(), buffer.begin(), buffer.end());
    }
    return out;
}

}  // namespace

TEST_CASE("every sound event has a short cue") {
    for (int i = 0; i < buraco::sound::kEventCount; ++i) {
        const Event e = Event(i);
        const std::vector<buraco::sound::Tone>& tones = buraco::sound::cue(e);
        CHECK(!tones.empty());
        CHECK(std::string(buraco::sound::name(e)) != "?");

        double total = 0.0;
        for (const buraco::sound::Tone& t : tones) {
            CHECK(t.freqHz >= 140.0 && t.freqHz <= 1200.0);
            CHECK(t.ms > 0.0 && t.ms <= 200.0);
            CHECK(t.gain > 0.0 && t.gain <= 1.0);
            total += t.ms;
        }
        CHECK(total <= 500.0);
    }
    // The sentinel event deliberately has no cue.
    CHECK(buraco::sound::cue(Event::Count).empty());
}

TEST_CASE("synth renders the same samples for the same cue") {
    buraco::sound::Synth a;
    buraco::sound::Synth b;
    a.setVolume(80);
    b.setVolume(80);
    a.play(Event::Batida);
    b.play(Event::Batida);

    const std::vector<int16_t> sa = renderAll(a);
    const std::vector<int16_t> sb = renderAll(b);
    CHECK(!sa.empty());
    CHECK(sa == sb);
    CHECK(anyNonZero(sa));
    CHECK(a.idle());
}

TEST_CASE("disabled and muted synths render pure silence") {
    buraco::sound::Synth disabled;
    disabled.setEnabled(false);
    disabled.play(Event::Batida);
    const std::vector<int16_t> a = renderAll(disabled);
    CHECK(!a.empty());
    CHECK(!anyNonZero(a));

    buraco::sound::Synth muted;
    muted.setVolume(0);
    muted.play(Event::Batida);
    const std::vector<int16_t> b = renderAll(muted);
    CHECK(!b.empty());
    CHECK(!anyNonZero(b));

    // The tone still finishes while muted, so re-enabling does not replay it.
    disabled.setEnabled(true);
    CHECK(disabled.idle());
}

TEST_CASE("synth keeps the samples inside the 16-bit range") {
    buraco::sound::Synth synth;
    synth.setVolume(100);
    synth.play(Event::Canastra);

    const std::vector<int16_t> samples = renderAll(synth);
    int peak = 0;
    for (int16_t v : samples) peak = std::max(peak, std::abs(int(v)));
    CHECK(peak > 1000);
    CHECK(peak <= 32767);
}

TEST_CASE("rejected actions can signal the error cue") {
    Game g;
    g.newMatch("Ana", 42u);
    g.takeEvents();

    g.signalError();
    CHECK(sameEvents(g.takeEvents(), {Event::Error}));
    CHECK(!g.hasEvents());
}

TEST_CASE("newMatch announces shuffle and deal") {
    Game g;
    g.newMatch("Ana", 42u);
    CHECK(sameEvents(g.takeEvents(), {Event::Shuffle, Event::Deal}));
    CHECK(g.takeEvents().empty());
    CHECK(!g.hasEvents());
}

TEST_CASE("drawing emits the matching cue") {
    Game g;
    g.newMatch("Ana", 42u);
    g.takeEvents();

    CHECK(g.draw(false));
    CHECK(sameEvents(g.takeEvents(), {Event::DrawDeck}));

    // Leave a card on the table, then buy the whole pile.
    g.requestDiscard();
    CHECK(g.discardCard(g.player(0).hand.back()));
    g.takeEvents();
    CHECK(g.draw(true));
    CHECK(sameEvents(g.takeEvents(), {Event::TakePile}));
}

TEST_CASE("laying a meld emits the meld cue") {
    Game g;
    g.newMatch("Ana", 42u);
    g.draw(false);
    g.takeEvents();

    std::vector<buraco::Card>& hand = g.player(0).hand;
    hand.clear();
    hand.push_back(makeCard(1));   // A of diamonds
    hand.push_back(makeCard(2));   // 2 of diamonds, wild
    hand.push_back(makeCard(3));   // 3 of diamonds
    hand.push_back(makeCard(53));  // joker, so the hand is not emptied

    CHECK(g.layMeld({makeCard(1), makeCard(2), makeCard(3)}).empty());
    CHECK(sameEvents(g.takeEvents(), {Event::LayMeld}));
}

TEST_CASE("completing a canastra adds its cue") {
    Game g;
    g.newMatch("Ana", 7u);
    g.draw(false);
    g.takeEvents();

    std::vector<buraco::Card> run;
    for (uint8_t id = 1; id <= 13; ++id) run.push_back(makeCard(id));

    std::vector<buraco::Card>& hand = g.player(0).hand;
    hand = run;
    hand.push_back(makeCard(53));  // keeps one card after laying the run

    CHECK(g.layMeld(run).empty());
    CHECK(sameEvents(g.takeEvents(), {Event::LayMeld, Event::Canastra}));
}

TEST_CASE("discarding into the morto emits discard then morto") {
    Game g;
    g.newMatch("Ana", 42u);
    g.draw(false);
    g.requestDiscard();
    g.takeEvents();

    buraco::PlayerState& p = g.player(0);
    const buraco::Card last = p.hand.back();
    p.hand = {last};

    CHECK(g.discardCard(last));
    CHECK(sameEvents(g.takeEvents(), {Event::Discard, Event::Morto}));
    CHECK_EQ(p.hand.size(), size_t(buraco::MORTO_HALF));
}

TEST_CASE("discarding the last card with no morto emits batida") {
    Game g;
    g.newMatch("Ana", 42u);
    g.draw(false);
    g.requestDiscard();
    g.takeEvents();

    buraco::PlayerState& p = g.player(0);
    p.tookMorto = true;  // no morto left for this player
    const buraco::Card last = p.hand.back();
    p.hand = {last};

    CHECK(g.discardCard(last));
    CHECK(sameEvents(g.takeEvents(), {Event::Discard, Event::Batida}));
    CHECK_EQ(int(g.phase()), int(Phase::HandOver));
}
