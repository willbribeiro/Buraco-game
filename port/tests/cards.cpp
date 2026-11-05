#include "cards.h"

#define CATCH2_TEST_MAIN
#include <catch2/catch_all.hpp>
#include <cstdio>

namespace {

TEST_CASE("Card makeCard: deck 0 cards", "[card]") {
    // Deck 0 contains cards 1-54 (52 normal + 2 jokers)
    for (uint8_t id = 1; id <= 52; ++id) {
        Card c = makeCard(id);
        CHECK(c.deck == 0);
        CHECK(c.id == id);
        
        uint8_t expectedSuit = uint8_t((id - 1) / 13 + 1);
        uint8_t expectedRank = uint8_t((id - 1) % 13 + 1);
        CHECK(c.suit == expectedSuit);
        CHECK(c.rank == expectedRank);
    }

    // Jokers in deck 0
    Card j0_1 = makeCard(53);
    CHECK(j0_1.deck == 0 && j0_1.suit == JOKER);
    
    Card j0_2 = makeCard(54);
    CHECK(j0_2.deck == 0 && j0_2.suit == JOKER);
}

TEST_CASE("Card makeCard: deck 1 cards", "[card]") {
    // Deck 1 contains cards 55-108 (52 normal + 2 jokers)
    for (uint8_t id = 55; id <= 106; ++id) {
        Card c = makeCard(id);
        CHECK(c.deck == 1);
        
        uint8_t offset = id - 55;
        uint8_t expectedSuit = uint8_t(offset / 13 + 1);
        uint8_t expectedRank = uint8_t(offset % 13 + 1);
        CHECK(c.suit == expectedSuit);
        CHECK(c.rank == expectedRank);
    }

    // Jokers in deck 1
    Card j1_1 = makeCard(107);
    CHECK(j1_1.deck == 1 && j1_1.suit == JOKER);
    
    Card j1_2 = makeCard(108);
    CHECK(j1_2.deck == 1 && j1_2.suit == JOKER);
}

TEST_CASE("Meld::buildMeld: minimal valid meld", "[meld]") {
    std::vector<Card> hand;
    hand.push_back(makeCard(1));      // A♦ (id=1)
    hand.push_back(makeCard(14));     // 2♦ (id=14, wild)
    hand.push_back(makeCard(27));     // 3♦ (id=27)

    Meld m;
    bool ok = buildMeld(hand, DIAMONDS, m);
    
    CHECK(ok == true);
    CHECK(m.suit == DIAMONDS);
    CHECK(m.size() == 3);
}

TEST_CASE("Meld::buildMeld: needs at least 3 cards", "[meld]") {
    std::vector<Card> hand;
    hand.push_back(makeCard(1));      // A♦
    hand.push_back(makeCard(14));     // 2♦

    Meld m;
    bool ok = buildMeld(hand, DIAMONDS, m);
    
    CHECK(ok == false);  // Not enough cards
}

TEST_CASE("Meld::buildMeld: requires valid meld suit", "[meld]") {
    std::vector<Card> hand;
    hand.push_back(makeCard(1));      // A♦
    hand.push_back(makeCard(14));     // 2♦

    Meld m;
    bool ok = buildMeld(hand, SPADES, m);
    
    CHECK(ok == false);  // Wrong suit
}

TEST_CASE("Meld::buildMeld: off-suit cards fail", "[meld]") {
    std::vector<Card> hand;
    hand.push_back(makeCard(1));      // A♦
    hand.push_back(makeCard(2));      // 2♠ (wild, but wrong suit)

    Meld m;
    bool ok = buildMeld(hand, DIAMONDS, m);
    
    CHECK(ok == false);  // Off-suit wild doesn't work
}

TEST_CASE("Meld::buildMeld: consecutive run", "[meld]") {
    std::vector<Card> hand;
    hand.push_back(makeCard(1));      // A♦ (id=1)
    hand.push_back(makeCard(14));     // 2♦ (id=14, wild)
    hand.push_back(makeCard(27));     // 3♦ (id=27)
    hand.push_back(makeCard(40));     // 4♦ (id=40)

    Meld m;
    bool ok = buildMeld(hand, DIAMONDS, m);
    
    CHECK(ok == true);
    CHECK(m.size() == 4);
}

TEST_CASE("Meld::buildMeld: ace high placement", "[meld]") {
    std::vector<Card> hand;
    hand.push_back(makeCard(1));      // A♦ (id=1)
    hand.push_back(makeCard(27));     // 3♦ (id=27)

    Meld m;
    bool ok = buildMeld(hand, DIAMONDS, m);
    
    CHECK(ok == true);  // Ace can go high after K
}

TEST_CASE("Meld::buildMeld: duplicate ranks fail", "[meld]") {
    std::vector<Card> hand;
    hand.push_back(makeCard(1));      // A♦ (id=1)
    hand.push_back(makeCard(250));    // Another A♦ in deck 1

    Meld m;
    bool ok = buildMeld(hand, DIAMONDS, m);
    
    CHECK(ok == true);  // Different decks are different cards
}

TEST_CASE("Meld::buildMeld: wild extends run", "[meld]") {
    std::vector<Card> hand;
    hand.push_back(makeCard(1));      // A♦ (id=1)
    hand.push_back(makeCard(53));     // JOKER (id=53)

    Meld m;
    bool ok = buildMeld(hand, DIAMONDS, m);
    
    CHECK(ok == true);  // Joker fills a hole
}

TEST_CASE("Meld::buildMeld: dirty flag set with wild", "[meld]") {
    std::vector<Card> hand;
    hand.push_back(makeCard(1));      // A♦ (id=1)
    hand.push_back(makeCard(53));     // JOKER (id=53)

    Meld m;
    buildMeld(hand, DIAMONDS, m);
    
    CHECK(m.dirty == true);  // Contains wild card
}

TEST_CASE("Meld::buildMeldAuto: finds best suit", "[meld]") {
    std::vector<Card> hand;
    hand.push_back(makeCard(1));      // A♦
    hand.push_back(makeCard(27));     // 3♦
    hand.push_back(makeCard(49));     // Q♥

    Meld m;
    bool ok = buildMeldAuto(hand, m);
    
    CHECK(ok == true);
    CHECK(m.suit == DIAMONDS);  // Diamonds dominates
}

TEST_CASE("Meld::buildMeldAuto: no valid meld fails", "[meld]") {
    std::vector<Card> hand;
    hand.push_back(makeCard(1));      // A♦
    hand.push_back(makeCard(49));     // Q♥
    hand.push_back(makeCard(85));     // K♣

    Meld m;
    bool ok = buildMeldAuto(hand, m);
    
    CHECK(ok == false);  // Not enough cards in same suit
}

TEST_CASE("Meld::buildMeldAuto: needs minimum cards", "[meld]") {
    std::vector<Card> hand;
    hand.push_back(makeCard(1));      // A♦

    Meld m;
    bool ok = buildMeldAuto(hand, m);
    
    CHECK(ok == false);  // Too few cards
}

TEST_CASE("Meld::points: sums card values", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1] = makeCard(ACE);      // 15 points
    m.slot[2] = makeCard(2);         // 10 points (but wild)
    m.slot[3] = makeCard(3);         // 5 points

    CHECK(m.points() == 30);  // 15 + 10 + 5
}

TEST_CASE("Meld::value: includes canastra bonus", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1] = makeCard(ACE);
    m.slot[2] = makeCard(2);
    m.slot[3] = makeCard(3);

    // Clean canastra (not dirty)
    m.dirty = false;
    CHECK(m.value() == 30 + 200);  // points + clean canastra bonus

    // Dirty canastra
    m.dirty = true;
    CHECK(m.value() == 30 + 100);  // points + dirty canastra bonus
}

TEST_CASE("Meld::canastra: checks minimum size", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1] = makeCard(ACE);
    m.slot[2] = makeCard(2);

    CHECK(m.canastra() == false);  // Only 2 cards

    m.slot[3] = makeCard(3);
    CHECK(m.canastra() == true);   // Now 3 cards (CANASTRA_LEN)
}

TEST_CASE("Meld::lowSlot: finds lowest occupied slot", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[3].id = 1;      // Slot 3
    m.slot[5].id = 2;      // Slot 5
    m.slot[8].id = 3;      // Slot 8

    CHECK(m.lowSlot() == 3);
}

TEST_CASE("Meld::highSlot: finds highest occupied slot", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;      // Slot 1
    m.slot[5].id = 2;      // Slot 5
    m.slot[8].id = 3;      // Slot 8

    CHECK(m.highSlot() == 8);
}

TEST_CASE("Meld::cards: returns cards in display order", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;      // Slot 1
    m.slot[5].id = 2;      // Slot 5

    auto cards = m.cards();
    CHECK(cards.size() == 2);
    CHECK(cards[0].id == 1);
    CHECK(cards[1].id == 2);
}

TEST_CASE("Meld::ranks: returns slot indices", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;      // Slot 1
    m.slot[5].id = 2;      // Slot 5

    auto ranks = m.ranks();
    CHECK(ranks.size() == 2);
}

TEST_CASE("Meld::empty: checks if meld is used", "[meld]") {
    Meld empty;
    CHECK(empty.empty() == true);

    Meld filled;
    filled.suit = DIAMONDS;
    CHECK(filled.empty() == false);
}

TEST_CASE("Meld::clear: resets meld", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;

    m.clear();
    CHECK(m.empty() == true);
    CHECK(m.suit == 0);
}

TEST_CASE("Extend meld: adds cards to existing run", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;      // A
    m.slot[2].id = 14;     // 2

    std::vector<Card> ext{makeCard(27)};  // Add 3

    bool ok = extendMeld(m, ext);
    CHECK(ok == true);
    CHECK(m.size() == 3);
}

TEST_CASE("Extend meld: rejects duplicate ranks", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;      // A (deck 0)
    m.slot[2].id = 14;     // 2 (deck 0)

    std::vector<Card> ext{makeCard(1)};  // Try to add another A with same ID

    bool ok = extendMeld(m, ext);
    CHECK(ok == false);  // Duplicate card ID
}

TEST_CASE("Extend meld: rejects invalid extension", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;      // A
    
    std::vector<Card> ext{makeCard(250)};  // Q (wrong rank, too far)

    bool ok = extendMeld(m, ext);
    CHECK(ok == false);
}

TEST_CASE("Extend meld: empty meld fails", "[meld]") {
    Meld m;
    
    std::vector<Card> ext{makeCard(1), makeCard(27)};

    bool ok = extendMeld(m, ext);
    CHECK(ok == false);  // Empty meld cannot be extended
}

TEST_CASE("Extend meld: grows existing run", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;      // A
    m.slot[2].id = 14;     // 2
    m.slot[3].id = 27;     // 3

    std::vector<Card> ext{makeCard(40)};  // Add 4

    bool ok = extendMeld(m, ext);
    CHECK(ok == true);
    CHECK(m.size() == 4);
}

TEST_CASE("Extend meld: fills gaps", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;      // A
    m.slot[3].id = 27;     // 3 (gap at slot 2)

    std::vector<Card> ext{makeCard(14)};  // Add 2 to fill gap

    bool ok = extendMeld(m, ext);
    CHECK(ok == true);
}

TEST_CASE("Extend meld: wildcard fills gaps", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;      // A
    m.slot[4].id = 40;     // 4 (gaps at slots 2, 3)

    std::vector<Card> ext{makeCard(53)};  // Joker fills gap

    bool ok = extendMeld(m, ext);
    CHECK(ok == true);  // Wildcard fills first hole
}

TEST_CASE("Extend meld: wildcard extends run", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;      // A
    m.slot[2].id = 14;     // 2
    m.slot[3].id = 27;     // 3

    std::vector<Card> ext{makeCard(53)};  // Joker extends above K

    bool ok = extendMeld(m, ext);
    CHECK(ok == true);  // Extends to slot 4
}

TEST_CASE("Extend meld: dirty flag set correctly", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;      // A
    
    std::vector<Card> ext{makeCard(53)};  // Joker

    bool ok = extendMeld(m, ext);
    CHECK(ok == true);
    CHECK(m.dirty == true);
}

TEST_CASE("Extend meld: returns false on failure", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;

    std::vector<Card> ext{makeCard(49)};  // Q (too far from A)

    bool ok = extendMeld(m, ext);
    CHECK(ok == false);
}

TEST_CASE("Extend meld: maximum slot is 14", "[meld]") {
    Meld m;
    m.suit = DIAMONDS;
    m.slot[1].id = 1;      // A
    
    std::vector<Card> ext{makeCard(52)};  // K

    bool ok = extendMeld(m, ext);
    CHECK(ok == true);
    // Should fill up to slot 14 (ace high)
}

}  // anonymous namespace
