// Card model for the Buraco port.
//
// Reconstructed from BURACO.EXE:
//   FUN_1000_1a9d  card id  -> {id, rank, suit, deck}
//   DS:0x96        rank    -> CP437 glyph  "A234567891JQKA"
//   DS:0x65        suit    -> CP437 glyph  1:♦ 2:♠ 3:♥ 4:♣ 5:'?'
//   DS:0x40        rank    -> printed width in text cells (10 is wide)
//   DS:0x76/0x86   rank    -> point value (A=15, 2=10, 3-7=5, 8-K=10, joker=20)
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace buraco {

// The original numbers suits 1..4 and uses 5 for jokers.
enum Suit : uint8_t {
    DIAMONDS = 1,
    SPADES = 2,
    HEARTS = 3,
    CLUBS = 4,
    JOKER = 5,
};

// Ranks 1..13 are A,2..K.  Rank 14 is the "ace high" slot of a sequence and
// rank 15 is the joker rank; both only ever appear inside a meld.
enum Rank : uint8_t {
    ACE = 1,
    JACK = 11,
    QUEEN = 12,
    KING = 13,
    ACE_HIGH = 14,
    JOKER_RANK = 15,
};

inline constexpr int DECK_SIZE = 108;
inline constexpr int HAND_SIZE = 11;
inline constexpr int MORTO_SIZE = 22;
inline constexpr int MORTO_HALF = 11;
inline constexpr int MELD_MIN = 3;
inline constexpr int CANASTRA_LEN = 7;

struct Card {
    uint8_t id = 0;    // 1..108 as stored by the original; 0 = empty
    uint8_t rank = 0;  // 1..13, 15 for jokers
    uint8_t suit = 0;  // 1..4, 5 for jokers
    uint8_t deck = 0;  // 0 or 1 (two physical decks)

    bool valid() const { return id != 0; }
    bool operator==(const Card& o) const { return id == o.id; }
    bool operator!=(const Card& o) const { return id != o.id; }
};

// id 1..54  -> deck 0 (52 cards + 2 jokers at 53,54)
// id 55..108-> deck 1 (52 cards + 2 jokers at 107,108)
Card makeCard(uint8_t id);

int cardPoints(const Card& c);

// 2s and jokers are wild: FUN_1000_1c6c returns true when rank == 2 or
// suit == 5.
bool isWild(const Card& c);

bool isRedSuit(const Card& c);

// CP437 glyph drawn on the card face.  A rank-10 card prints '1' followed by
// an extra '0' at the next cell (see ui::drawCard).
char rankGlyph(const Card& c);
char suitGlyph(const Card& c);

// Suit glyph from a bare suit number (used when only the meld suit matters).
char suitGlyphFor(uint8_t suit);

// Width in text cells: 2 normally, 3 for a ten.
int cardWidth(const Card& c);

std::string describe(const Card& c);  // "A♦", "10♠", "JOKER"

// ---------------------------------------------------------------------------
// Meld
// ---------------------------------------------------------------------------
// A meld is one suit, consecutive ranks, at least three cards.  Slot i holds
// the card for rank i (1..14, so an ace may sit low or high); slot 15 is the
// extra position above the top of the run, which is where wildcards go when
// they extend the sequence.
struct Meld {
    uint8_t suit = 0;                  // 0 = unused meld slot
    bool dirty = false;                // a wildcard substitutes for a real card
    std::array<Card, 16> slot{};       // index 1..15
    std::array<bool, 16> wild{};       // this slot is filled by a wildcard

    bool empty() const { return suit == 0; }
    void clear() { *this = Meld{}; }

    int size() const;
    int points() const;   // sum of card values, no canastra bonus
    int value() const;    // points + 500 real / 200 clean / 100 dirty canastra
    bool canastra() const { return size() >= CANASTRA_LEN; }

    // Real canastra: the complete A..K run of one suit with no wildcard.
    // The 2 is wild in a normal meld, but inside a real canastra the 2 of the
    // meld suit counts as its natural rank (A,2,...,K), which is the only way
    // the full run can exist in this deck.
    bool realCanastra() const;

    // Lowest/highest occupied slot (0 when the meld is empty).
    int lowSlot() const;
    int highSlot() const;

    // Cards in display order (by slot).
    std::vector<Card> cards() const;
    std::vector<int> ranks() const;    // slot indices, same order as cards()
};

// Fills `out` from `selection` using `suit` as the meld suit.  Returns false
// when the cards do not form a legal meld (gaps that wildcards cannot cover,
// duplicate ranks, mixed suits, fewer than three cards, ...).
bool buildMeld(const std::vector<Card>& selection, uint8_t suit, Meld& out);

// Picks the dominant suit of a selection (most natural cards, jokers ignored).
uint8_t dominantSuit(const std::vector<Card>& selection);

// Tries every candidate suit and returns the first legal interpretation.
bool buildMeldAuto(const std::vector<Card>& selection, Meld& out);

// Appends `selection` to an existing meld when the result stays a legal run.
bool extendMeld(Meld& meld, const std::vector<Card>& selection);

}  // namespace buraco
