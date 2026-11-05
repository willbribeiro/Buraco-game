#define CATCH2_TEST_MAIN
#include "cards.h"
#include "game.h"

#include <catch2/catch_all.hpp>
#include <cassert>
#include <cmath>
#include <cstdio>

namespace buraco {

// Helper to make tests deterministic
bool isDeterministicMode() {
#ifdef DETERMINISTIC_TEST
    return true;
#else
    return false;
#endif
}

void setDeterministicMode(bool on) {}

}  // namespace buraco

namespace {

TEST_CASE("Card creation from id", "[card]") {
    Card c = makeCard(1);
    CHECK(c.valid());
    CHECK(c.id == 1);
    CHECK(c.rank == ACE);
    CHECK(c.suit == DIAMONDS);
    CHECK(c.deck == 0);

    Card c2 = makeCard(52);
    CHECK(c2.suit == SPADES);
    CHECK(c2.rank == KING);

    // Joker in deck 0
    Card j1 = makeCard(53);
    CHECK(j1.suit == JOKER);
    CHECK(j1.rank == JOKER_RANK);

    // Joker in deck 1
    Card j2 = makeCard(107);
    CHECK(j2.deck == 1);
}

TEST_CASE("Card points", "[card]") {
    CHECK(cardPoints(makeCard(ACE)) == 15);
    CHECK(cardPoints(makeCard(2)) == 10);
    CHECK(cardPoints(makeCard(3)) == 5);
    CHECK(cardPoints(makeCard(7)) == 5);
    CHECK(cardPoints(makeCard(8)) == 10);
    CHECK(cardPoints(makeCard(KING)) == 10);

    // Joker is worth 20 points
    CHECK(cardPoints(makeCard(53)) == 20);

    // Invalid card returns 0
    Card invalid;
    invalid.id = 0;
    CHECK(cardPoints(invalid) == 0);
}

TEST_CASE("Wild card detection", "[card]") {
    // A 2 is wild in any suit
    CHECK(isWild(makeCard(2)) == true);      // 2♦
    CHECK(isWild(makeCard(17)) == true);    // 2♠
    CHECK(isWild(makeCard(30)) == true);    // 2♥

    // A regular card is not wild
    CHECK(isWild(makeCard(ACE)) == false);
    CHECK(isWild(makeCard(JACK)) == false);

    // Jokers are wild
    CHECK(isWild(makeCard(53)) == true);     // small joker
    CHECK(isWild(makeCard(107)) == true);   // large joker
}

TEST_CASE("Red suit detection", "[card]") {
    // Diamonds and hearts are red
    Card d = makeCard(ACE);
    d.suit = DIAMONDS;
    CHECK(isRedSuit(d) == true);

    Card h = makeCard(KING);
    h.suit = HEARTS;
    CHECK(isRedSuit(h) == true);

    // Clubs and spades are black
    Card c = makeCard(QUEEN);
    c.suit = CLUBS;
    CHECK(isRedSuit(c) == false);

    Card s = makeCard(JACK);
    s.suit = SPADES;
    CHECK(isRedSuit(s) == false);

    // Jokers are black
    Card j = makeCard(53);
    CHECK(isRedSuit(j) == false);
}

TEST_CASE("Rank glyph rendering", "[card]") {
    CHECK(rankGlyph(makeCard(ACE)) == 'A');
    CHECK(rankGlyph(makeCard(JACK)) == 'J');
    CHECK(rankGlyph(makeCard(QUEEN)) == 'Q');
    CHECK(rankGlyph(makeCard(KING)) == 'K');

    // Rank 10 prints as '1' (special case in UI)
    Card ten = makeCard(10);
    CHECK(rankGlyph(ten) == '1');

    // Joker rank
    CHECK(rankGlyph(makeCard(JOKER_RANK)) == '?');
}

TEST_CASE("Suit glyph rendering", "[card]") {
    // Using CP437 hex codes
    CHECK(suitGlyphFor(DIAMONDS) == '\x04');
    CHECK(suitGlyphFor(SPADS) == '\x06');
    CHECK(suitGlyphFor(HEARTS) == '\x03');
    CHECK(suitGlyphFor(CLUBS) == '\x05');

    // Joker glyph
    CHECK(suitGlyphFor(JOKER) == '?');
}

TEST_CASE("Card width calculation", "[card]") {
    // Regular cards are 2 cells wide
    CHECK(cardWidth(makeCard(ACE)) == 2);
    CHECK(cardWidth(makeCard(JACK)) == 2);

    // Ten is 3 cells wide (special case in UI)
    Card ten = makeCard(10);
    CHECK(cardWidth(ten) == 3);

    // Invalid card has width 2 by default
    CHECK(cardWidth(Card{}) == 2);
}

TEST_CASE("Describe card", "[card]") {
    // Regular cards
    CHECK(describe(makeCard(ACE)) == "A♦");
    CHECK(describe(makeCard(KING)) == "K♣");

    // Ten shows special formatting
    Card ten = makeCard(10);
    CHECK(describe(ten) == "10♠");

    // Jokers
    CHECK(describe(makeCard(53)) == "JOKER");
    CHECK(describe(makeCard(107)) == "JOKER");

    // Invalid card
    CHECK(describe(Card{}) == "--");
}

TEST_CASE("Meld size calculation", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;
    m.slot[2].id = 14;
    m.slot[3].id = 27;

    CHECK(m.size() == 3);

    // Empty meld
    Meld empty;
    CHECK(empty.size() == 0);
}

TEST_CASE("Meld points calculation", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1] = makeCard(ACE);
    m.slot[2] = makeCard(2);
    m.slot[3] = makeCard(3);

    CHECK(m.points() == 40);  // 15 + 10 + 5

    // Canastra bonus
    m.dirty = false;
    CHECK(m.value() == 40 + 200);  // points + canastra bonus
}

TEST_CASE("Meld low/high slot", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[3].id = 1;
    m.slot[5].id = 2;
    m.slot[8].id = 3;

    CHECK(m.lowSlot() == 3);
    CHECK(m.highSlot() == 8);

    // Single card meld
    Meld single;
    single.suit = DIAMONDS;
    single.slot[5].id = 14;
    CHECK(single.lowSlot() == 5);
    CHECK(single.highSlot() == 5);

    // Empty meld
    CHECK(Meld{}.lowSlot() == 0);
    CHECK(Meld{}.highSlot() == 0);
}

TEST_CASE("Meld cards extraction", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;
    m.slot[2].id = 2;
    m.slot[3].id = 3;

    auto cards = m.cards();
    CHECK(cards.size() == 3);
    CHECK(cards[0].id == 1);
    CHECK(cards[1].id == 2);
    CHECK(cards[2].id == 3);
}

TEST_CASE("Meld ranks extraction", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;   // ACE
    m.slot[4].id = 17;  // 2
    m.slot[7].id = 30;  // 3

    auto ranks = m.ranks();
    CHECK(ranks.size() == 3);
}

TEST_CASE("Dominant suit detection", "[meld]") {
    std::vector<Card> selection;
    selection.push_back(makeCard(1));   // A♦
    selection.push_back(makeCard(14));  // 2♦
    selection.push_back(makeCard(27));  // 3♦

    uint8_t suit = dominantSuit(selection);
    CHECK(suit == DIAMONDS);

    // Mixed suits with one dominating
    std::vector<Card> mixed;
    mixed.push_back(makeCard(1));   // A♦
    mixed.push_back(makeCard(10));  // 10♠
    mixed.push_back(makeCard(23));  // Q♣

    suit = dominantSuit(mixed);
    CHECK(suit == DIAMONDS);
}

TEST_CASE("Extend meld basic", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;   // A
    m.slot[2].id = 14;  // 2
    m.dirty = true;

    std::vector<Card> ext{makeCard(27)};  // Add 3

    bool ok = extendMeld(m, ext);
    CHECK(ok == true);
    CHECK(m.size() == 3);
}

TEST_CASE("Empty meld has no cards", "[meld]") {
    Meld empty;
    CHECK(empty.empty() == true);

    Meld filled;
    filled.suit = DIAMONDS;
    filled.slot[1].id = 1;
    CHECK(filled.empty() == false);
}

TEST_CASE("Empty meld clear", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;

    m.clear();
    CHECK(m.empty() == true);
    CHECK(m.suit == 0);
}

TEST_CASE("Card ID uniqueness", "[card]") {
    std::set<Card> cards;
    for (uint8_t id = 1; id <= DECK_SIZE; ++id) {
        Card c = makeCard(id);
        cards.insert(c);
    }
    CHECK(cards.size() == DECK_SIZE);
}

TEST_CASE("Two decks have distinct cards", "[card]") {
    // First deck: 1-52 normal + jokers 53,54
    Card c1 = makeCard(1);
    Card c2 = makeCard(52);
    CHECK(c1.deck == 0);
    CHECK(c2.deck == 0);

    // Second deck: same ranks/suits but deck=1
    Card d1 = makeCard(55);  // Same as id=1 in deck 1
    d1.id = 55;
    CHECK(d1.deck == 1);
}

}  // anonymous namespace
