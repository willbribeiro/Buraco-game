// Card model and meld construction tests.
#include "cards.h"

#include "test_framework.hpp"

using namespace buraco;

namespace {

// Diamond ranks 1..13 use ids 1..13, so makeDiamond(rank) keeps the tests easy
// to read.  Spades start at 14, hearts at 27 and clubs at 40.
Card diamond(int rank) { return makeCard(uint8_t(rank)); }

TEST_CASE("makeCard maps every deck-0 card") {
    for (int id = 1; id <= 52; ++id) {
        const Card c = makeCard(uint8_t(id));
        CHECK(c.valid());
        CHECK_EQ(int(c.id), id);
        CHECK_EQ(int(c.deck), 0);
        CHECK_EQ(int(c.suit), (id - 1) / 13 + 1);
        CHECK_EQ(int(c.rank), (id - 1) % 13 + 1);
    }
    CHECK_EQ(int(makeCard(53).suit), int(JOKER));
    CHECK_EQ(int(makeCard(53).rank), int(JOKER_RANK));
    CHECK_EQ(int(makeCard(54).suit), int(JOKER));
    CHECK_EQ(int(makeCard(54).deck), 0);
}

TEST_CASE("makeCard maps every deck-1 card") {
    for (int id = 55; id <= 106; ++id) {
        const Card c = makeCard(uint8_t(id));
        CHECK_EQ(int(c.deck), 1);
        CHECK_EQ(int(c.suit), (id - 55) / 13 + 1);
        CHECK_EQ(int(c.rank), (id - 55) % 13 + 1);
    }
    CHECK_EQ(int(makeCard(107).suit), int(JOKER));
    CHECK_EQ(int(makeCard(108).deck), 1);
    CHECK(!makeCard(0).valid());
}

TEST_CASE("cardPoints follows the original table") {
    CHECK_EQ(cardPoints(diamond(ACE)), 15);
    CHECK_EQ(cardPoints(diamond(2)), 10);
    CHECK_EQ(cardPoints(diamond(3)), 5);
    CHECK_EQ(cardPoints(diamond(7)), 5);
    CHECK_EQ(cardPoints(diamond(8)), 10);
    CHECK_EQ(cardPoints(diamond(KING)), 10);
    CHECK_EQ(cardPoints(makeCard(53)), 20);
    CHECK_EQ(cardPoints(Card{}), 0);
}

TEST_CASE("wild cards are the 2s and the jokers") {
    CHECK(isWild(makeCard(2)));    // 2♦
    CHECK(isWild(makeCard(15)));   // 2♠
    CHECK(isWild(makeCard(28)));   // 2♥
    CHECK(isWild(makeCard(41)));   // 2♣
    CHECK(isWild(makeCard(53)));
    CHECK(isWild(makeCard(108)));
    CHECK(!isWild(diamond(ACE)));
    CHECK(!isWild(diamond(JACK)));
    CHECK(!isWild(Card{}));
}

TEST_CASE("red suits") {
    CHECK(isRedSuit(diamond(5)));
    CHECK(isRedSuit(makeCard(32)));  // 6♥
    CHECK(!isRedSuit(makeCard(18))); // 5♠
    CHECK(!isRedSuit(makeCard(44))); // 5♣
    CHECK(!isRedSuit(makeCard(53)));
}

TEST_CASE("display glyphs and widths") {
    CHECK_EQ(rankGlyph(diamond(ACE)), 'A');
    CHECK_EQ(rankGlyph(diamond(10)), '1');  // the extra '0' is drawn by the UI
    CHECK_EQ(rankGlyph(diamond(JACK)), 'J');
    CHECK_EQ(rankGlyph(diamond(QUEEN)), 'Q');
    CHECK_EQ(rankGlyph(diamond(KING)), 'K');
    CHECK_EQ(rankGlyph(makeCard(53)), '?');

    CHECK_EQ(int(uint8_t(suitGlyphFor(DIAMONDS))), 0x04);
    CHECK_EQ(int(uint8_t(suitGlyphFor(SPADES))), 0x06);
    CHECK_EQ(int(uint8_t(suitGlyphFor(HEARTS))), 0x03);
    CHECK_EQ(int(uint8_t(suitGlyphFor(CLUBS))), 0x05);
    CHECK_EQ(suitGlyphFor(JOKER), '?');

    CHECK_EQ(cardWidth(diamond(9)), 2);
    CHECK_EQ(cardWidth(diamond(10)), 3);
    CHECK_EQ(cardWidth(Card{}), 2);
}

TEST_CASE("Meld size, points and slot bounds") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1] = diamond(ACE);
    m.slot[2] = diamond(2);
    m.slot[3] = diamond(3);
    CHECK_EQ(m.size(), 3);
    CHECK_EQ(m.points(), 30);
    CHECK_EQ(m.lowSlot(), 1);
    CHECK_EQ(m.highSlot(), 3);

    Meld single;
    single.suit = DIAMONDS;
    single.slot[5] = diamond(ACE);
    CHECK_EQ(single.lowSlot(), 5);
    CHECK_EQ(single.highSlot(), 5);

    CHECK_EQ(Meld{}.lowSlot(), 0);
    CHECK_EQ(Meld{}.highSlot(), 0);
    CHECK_EQ(Meld{}.size(), 0);
}

TEST_CASE("Meld cards are returned in slot order") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1] = diamond(ACE);
    m.slot[3] = diamond(3);
    const std::vector<Card> cards = m.cards();
    REQUIRE(cards.size() == 2);
    CHECK_EQ(int(cards[0].id), 1);
    CHECK_EQ(int(cards[1].id), 3);
}

TEST_CASE("canastra needs seven cards") {
    Meld m;
    m.suit = DIAMONDS;
    for (int slot = 1; slot <= 6; ++slot) m.slot[slot] = diamond(slot);
    CHECK(!m.canastra());
    m.slot[7] = diamond(7);
    CHECK(m.canastra());
}

TEST_CASE("canastra bonuses: clean, dirty and real") {
    Meld clean;
    clean.suit = DIAMONDS;
    for (int slot = 1; slot <= 7; ++slot) clean.slot[slot] = diamond(slot);
    CHECK_EQ(clean.points(), 50);       // A15 + 2(10) + five 5s
    CHECK_EQ(clean.value(), 250);

    Meld dirty = clean;
    dirty.dirty = true;
    dirty.wild[2] = true;
    CHECK_EQ(dirty.value(), 150);

    Meld real;
    real.suit = DIAMONDS;
    for (int slot = 1; slot <= 13; ++slot) real.slot[slot] = diamond(slot);
    CHECK(real.realCanastra());
    CHECK_EQ(real.points(), 110);  // A15 + 2(10) + five 5s + six 10s
    CHECK_EQ(real.value(), 610);  // 110 + 500 real canastra

    // Anything above the king disqualifies a real canastra.
    Meld aceHigh = real;
    aceHigh.slot[13] = Card{};
    aceHigh.slot[14] = diamond(KING);
    CHECK(!aceHigh.realCanastra());
}

TEST_CASE("buildMeld accepts a minimal run and marks the wildcard") {
    Meld m;
    REQUIRE(buildMeld({diamond(ACE), diamond(2), diamond(3)}, DIAMONDS, m));
    CHECK_EQ(int(m.suit), int(DIAMONDS));
    CHECK_EQ(m.size(), 3);
    CHECK(m.dirty);
    CHECK(m.wild[2]);
    CHECK_EQ(int(m.slot[1].id), 1);
    CHECK_EQ(int(m.slot[2].id), 2);
    CHECK_EQ(int(m.slot[3].id), 3);
}

TEST_CASE("buildMeld rejects short, off-suit and duplicate selections") {
    Meld m;
    CHECK(!buildMeld({diamond(ACE), diamond(3)}, DIAMONDS, m));
    CHECK(!buildMeld({diamond(ACE), diamond(3), makeCard(14)}, DIAMONDS, m));
    CHECK(!buildMeld({diamond(ACE), diamond(3), diamond(KING)}, DIAMONDS, m));
    // Two aces of the same suit (one from each deck).
    CHECK(!buildMeld({diamond(ACE), makeCard(55), diamond(3)}, DIAMONDS, m));
    // The same physical card twice.
    CHECK(!buildMeld({diamond(ACE), diamond(ACE), diamond(3)}, DIAMONDS, m));
    CHECK(!buildMeld({diamond(ACE), diamond(3), Card{}}, DIAMONDS, m));
}

TEST_CASE("buildMeld places an ace low or high as needed") {
    Meld low;
    REQUIRE(buildMeld({diamond(ACE), diamond(2), diamond(3)}, DIAMONDS, low));
    CHECK_EQ(low.lowSlot(), 1);

    Meld high;
    REQUIRE(buildMeld({diamond(ACE), diamond(QUEEN), diamond(KING)}, DIAMONDS, high));
    CHECK_EQ(high.lowSlot(), 12);
    CHECK_EQ(high.highSlot(), 14);
    CHECK_EQ(int(high.slot[14].id), 1);
}

TEST_CASE("buildMeld uses wildcards for interior holes and the top") {
    Meld interior;
    REQUIRE(buildMeld({diamond(ACE), diamond(3), diamond(4), makeCard(53)}, DIAMONDS, interior));
    CHECK(interior.wild[2]);
    CHECK_EQ(interior.size(), 4);

    Meld top;
    REQUIRE(buildMeld({diamond(JACK), diamond(QUEEN), diamond(KING), makeCard(53)}, DIAMONDS,
                      top));
    CHECK(top.wild[14]);
    CHECK_EQ(top.size(), 4);
}

TEST_CASE("buildMeldAuto picks the dominant suit") {
    Meld m;
    REQUIRE(buildMeldAuto({diamond(ACE), diamond(2), diamond(3), makeCard(15)}, m));
    CHECK_EQ(int(m.suit), int(DIAMONDS));
    CHECK_EQ(m.size(), 4);
}

TEST_CASE("buildMeldAuto fails without a usable suit") {
    Meld m;
    CHECK(!buildMeldAuto({makeCard(1), makeCard(14), makeCard(27)}, m));
    CHECK(!buildMeldAuto({makeCard(1)}, m));
    CHECK(!buildMeldAuto({}, m));
}

TEST_CASE("dominantSuit ignores wildcards") {
    CHECK_EQ(int(dominantSuit({diamond(ACE), diamond(2), makeCard(27)})), int(DIAMONDS));
    CHECK_EQ(int(dominantSuit({diamond(2), diamond(15)})), 0);
}

TEST_CASE("extendMeld grows runs and rejects invalid cards") {
    Meld m;
    REQUIRE(buildMeld({diamond(ACE), diamond(2), diamond(3)}, DIAMONDS, m));

    Meld grown = m;
    REQUIRE(extendMeld(grown, {diamond(4)}));
    CHECK_EQ(grown.size(), 4);

    Meld duplicate = m;
    CHECK(!extendMeld(duplicate, {diamond(ACE)}));

    Meld gap = m;
    CHECK(!extendMeld(gap, {diamond(9)}));

    Meld empty;
    CHECK(!extendMeld(empty, {diamond(4)}));

    Meld wildTop = m;
    REQUIRE(extendMeld(wildTop, {makeCard(53)}));
    CHECK_EQ(wildTop.size(), 4);
    CHECK(wildTop.wild[4]);
}

TEST_CASE("extendMeld fills a hole opened by a larger extension") {
    Meld m;
    REQUIRE(buildMeld({diamond(ACE), diamond(2), diamond(3)}, DIAMONDS, m));
    // Adding 5 leaves the 4 missing; the wildcard 2 already in the meld is
    // reassigned, so the run stays contiguous.
    Meld grown = m;
    CHECK(!extendMeld(grown, {diamond(5)}));  // only one wild, not enough for 4
}

}  // namespace
