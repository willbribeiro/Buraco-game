#include "cards.h"

#include <algorithm>
#include <cstdio>

namespace buraco {
namespace {

// Highest slot a run can occupy: A..K (13) plus the ace played above the king
// (14).  Slot 15 stays free so a wildcard can never invent a rank.
constexpr int MAX_SLOT = 14;

// DS:0x96 - index by rank, 1..15.
constexpr char kRankGlyph[16] = {
    ' ', 'A', '2', '3', '4', '5', '6', '7', '8', '9', '1', 'J', 'Q', 'K', 'A', '?',
};

// DS:0x65 – index by suit, 1..5.
constexpr char kSuitGlyph[6] = {' ', '\x04', '\x06', '\x03', '\x05', '?'};

// DS:0x76 – index by rank.
constexpr int kPoints[16] = {0, 15, 10, 5, 5, 5, 5, 5, 10, 10, 10, 10, 10, 10, 15, 20};

}  // namespace

Card makeCard(uint8_t id) {
    Card c;
    c.id = id;
    if (id == 0) return c;
    if (id <= 52) {
        c.deck = 0;
        c.suit = uint8_t((id - 1) / 13 + 1);
        c.rank = uint8_t((id - 1) % 13 + 1);
    } else if (id <= 54) {
        c.deck = 0;
        c.suit = JOKER;
        c.rank = JOKER_RANK;
    } else if (id <= 106) {
        c.deck = 1;
        c.suit = uint8_t((id - 55) / 13 + 1);
        c.rank = uint8_t((id - 55) % 13 + 1);
    } else {
        c.deck = 1;
        c.suit = JOKER;
        c.rank = JOKER_RANK;
    }
    return c;
}

int cardPoints(const Card& c) {
    if (!c.valid()) return 0;
    return kPoints[c.rank & 15];
}

bool isWild(const Card& c) { return c.valid() && (c.rank == 2 || c.suit == JOKER); }

bool isRedSuit(const Card& c) { return c.suit == DIAMONDS || c.suit == HEARTS; }

char rankGlyph(const Card& c) { return kRankGlyph[c.rank & 15]; }

char suitGlyph(const Card& c) { return kSuitGlyph[c.suit % 6]; }

char suitGlyphFor(uint8_t suit) { return kSuitGlyph[suit % 6]; }

int cardWidth(const Card& c) { return c.rank == 10 ? 3 : 2; }

std::string describe(const Card& c) {
    if (!c.valid()) return "--";
    if (c.suit == JOKER) return "JOKER";
    char buf[8];
    std::snprintf(buf, sizeof buf, "%c%c", rankGlyph(c), suitGlyph(c));
    return buf;
}

int Meld::size() const {
    int n = 0;
    for (int i = 1; i <= 15; ++i)
        if (slot[i].valid()) ++n;
    return n;
}

int Meld::points() const {
    int sum = 0;
    for (int i = 1; i <= 15; ++i)
        if (slot[i].valid()) sum += cardPoints(slot[i]);
    return sum;
}

int Meld::value() const {
    int v = points();
    if (canastra()) v += dirty ? 100 : 200;
    return v;
}

int Meld::lowSlot() const {
    for (int i = 1; i <= 15; ++i)
        if (slot[i].valid()) return i;
    return 0;
}

int Meld::highSlot() const {
    for (int i = 15; i >= 1; --i)
        if (slot[i].valid()) return i;
    return 0;
}

std::vector<Card> Meld::cards() const {
    std::vector<Card> out;
    for (int i = 1; i <= 15; ++i)
        if (slot[i].valid()) out.push_back(slot[i]);
    return out;
}

std::vector<int> Meld::ranks() const {
    std::vector<int> out;
    for (int i = 1; i <= 15; ++i)
        if (slot[i].valid()) out.push_back(i);
    return out;
}

uint8_t dominantSuit(const std::vector<Card>& selection) {
    int count[6] = {0, 0, 0, 0, 0, 0};
    for (const Card& c : selection)
        if (c.valid() && !isWild(c)) ++count[c.suit];

    uint8_t best = 0;
    int bestCount = 0;
    for (uint8_t s = 1; s <= 4; ++s) {
        if (count[s] > bestCount) {
            best = s;
            bestCount = count[s];
        }
    }
    return best;
}

namespace {

// Places every card of `selection` in `out` assuming `suit` is the meld suit.
bool place(const std::vector<Card>& selection, uint8_t suit, Meld& out) {
    out.clear();
    out.suit = suit;

    // Guard against the same physical card entering a meld twice.
    for (size_t i = 0; i < selection.size(); ++i)
        for (size_t j = i + 1; j < selection.size(); ++j)
            if (selection[i].id == selection[j].id) return false;

    std::vector<Card> naturals;
    std::vector<Card> wilds;
    for (const Card& c : selection) {
        if (!c.valid()) return false;
        // A 2 or a joker is always a wildcard, whatever its suit: the original
        // tests rank == 2 || suit == 5 (FUN_1000_1c6c), so a 2♠ in a spades
        // meld is a substitute, never a natural two.
        if (isWild(c)) {
            wilds.push_back(c);
        } else if (c.suit == suit) {
            naturals.push_back(c);
        } else {
            return false;  // an off-suit card that is not a wildcard
        }
    }
    if (naturals.empty()) return false;
    if (naturals.size() + wilds.size() < size_t(MELD_MIN)) return false;

    // Low and high bounds for each possible placement of an ace.
    struct Placement {
        int lo, hi;
        bool aceHigh;
    };
    std::vector<Placement> placements;
    bool hasAce = false;
    int loNonAce = 99, hiNonAce = 0;
    for (const Card& c : naturals) {
        if (c.rank == ACE) {
            if (hasAce) return false;  // two aces of the same suit
            hasAce = true;
        } else {
            loNonAce = std::min(loNonAce, int(c.rank));
            hiNonAce = std::max(hiNonAce, int(c.rank));
        }
    }
    if (hasAce) {
        placements.push_back({std::min(1, loNonAce < 99 ? loNonAce : 14),
                              std::max(1, hiNonAce), false});
        placements.push_back({loNonAce < 99 ? loNonAce : 14, 14, true});
    } else {
        placements.push_back({loNonAce, hiNonAce, false});
    }

    const int wildCount = int(wilds.size());
    bool ok = false;
    Placement chosen{0, 0, false};
    int bestHoles = 1 << 20;
    for (const Placement& p : placements) {
        if (p.lo < 1 || p.hi > 14 || p.lo > p.hi) continue;
        int holes = 0;
        for (int r = p.lo; r <= p.hi; ++r) {
            bool filled = false;
            for (const Card& c : naturals)
                if ((c.rank == ACE && p.aceHigh ? 14 : int(c.rank)) == r) filled = true;
            if (!filled) ++holes;
        }
        if (holes > wildCount) continue;
        int extra = wildCount - holes;
        if (p.hi + extra > MAX_SLOT) continue;
        if (holes < bestHoles) {
            bestHoles = holes;
            chosen = p;
            ok = true;
        }
    }
    if (!ok) return false;

    auto slotOf = [&](const Card& c) -> int {
        if (c.rank == ACE) return chosen.aceHigh ? 14 : 1;
        return c.rank;
    };

    for (const Card& c : naturals) {
        int s = slotOf(c);
        if (out.slot[s].valid()) return false;  // duplicate rank
        // Melds must only hold canonical cards.  The AI briefly marks an ace
        // with rank 14 so it sorts above the king; the save file only stores
        // ids, and a reloaded ace would get rank 1 again.  Normalising here
        // keeps the in-memory and reloaded layouts identical.
        out.slot[s] = makeCard(c.id);
    }

    int next = chosen.lo;
    for (const Card& w : wilds) {
        while (next <= chosen.hi && out.slot[next].valid()) ++next;
        int target;
        if (next <= chosen.hi) {
            target = next++;  // fills an interior hole
        } else {
            target = ++chosen.hi;  // extends the run above
        }
        if (target < 1 || target > MAX_SLOT || out.slot[target].valid()) return false;
        out.slot[target] = w;
        out.wild[target] = true;
    }

    for (int i = 1; i <= 15; ++i)
        if (out.slot[i].valid() && out.wild[i]) out.dirty = true;

    // The run must be contiguous.
    int lo = out.lowSlot(), hi = out.highSlot();
    if (lo == 0) return false;
    for (int i = lo; i <= hi; ++i)
        if (!out.slot[i].valid()) return false;

    return true;
}

}  // namespace

bool buildMeld(const std::vector<Card>& selection, uint8_t suit, Meld& out) {
    return place(selection, suit, out);
}

bool buildMeldAuto(const std::vector<Card>& selection, Meld& out) {
    if (selection.size() < size_t(MELD_MIN)) return false;
    uint8_t first = dominantSuit(selection);
    std::vector<uint8_t> order;
    if (first) order.push_back(first);
    for (uint8_t s = 1; s <= 4; ++s)
        if (s != first) order.push_back(s);

    for (uint8_t s : order) {
        Meld m;
        if (place(selection, s, m)) {
            out = m;
            return true;
        }
    }
    return false;
}

bool extendMeld(Meld& meld, const std::vector<Card>& selection) {
    if (meld.empty()) return false;
    std::vector<Card> merged = meld.cards();
    // A card can never be in the meld and in the selection at the same time.
    for (const Card& c : selection)
        for (const Card& o : merged)
            if (c.id == o.id) return false;
    merged.insert(merged.end(), selection.begin(), selection.end());
    if (merged.size() <= size_t(meld.size())) return false;

    Meld m;
    if (!place(merged, meld.suit, m)) return false;
    meld = m;
    return true;
}

}  // namespace buraco
