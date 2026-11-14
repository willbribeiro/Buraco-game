#include "game.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>

namespace buraco {

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------
// Line-oriented text so a save can be diffed and eyeballed.  Every card is
// stored by its original id (1..108), which is enough to rebuild rank/suit/
// deck through makeCard().  Melds store the 15 slots plus their wild flags.
//
//   BURACO-SAVE 1
//   hand <n>          current <seat>     phase <0..4>    prompt <0..2>
//   rng <u32>         wentout <seat> <byMeld 0|1>        timer <seconds>
//   ctrl <c0> <c1>    optional; 0 human, 1 AI, 2 remote (absent = solo)
//   name <seat> <text to end of line>
//   player <seat> <tookMorto> <wentOut> <lastHandPoints> <total> <n> <ids...>
//   melds <seat> <count>
//   meld <seat> <suit> <dirty> <15 ids> <15 wild flags>
//   monte <n> <ids...>   mesa <n> <ids...>   morto <n> <ids...>
//   msg <text to end of line>

namespace {

constexpr const char* kSaveMagic = "BURACO-SAVE 1";

bool validId(int id) { return id == 0 || (id >= 1 && id <= DECK_SIZE); }

// True when no token is left on the line.  Free-text fields (`name`, `msg`)
// deliberately skip this check.
bool atEnd(std::istringstream& in) {
    in >> std::ws;
    return in.peek() == std::char_traits<char>::eof();
}

bool claimCard(const Card& c, std::array<int, DECK_SIZE + 1>& seen) {
    if (!c.valid() || c.id > DECK_SIZE) return false;
    return ++seen[c.id] == 1;
}

// A saved meld must look like one the game could have built: one of the four
// suits, a contiguous run of at least MELD_MIN cards, and wild flags only on
// wildcards (2s and jokers).  Natural 2s are accepted in slot 2 for old saves
// and for the real A..K canastra, where the 2 is part of the run.
bool validMeld(const Meld& m, std::array<int, DECK_SIZE + 1>& seen) {
    if (m.suit < DIAMONDS || m.suit > CLUBS) return false;
    const int lo = m.lowSlot();
    const int hi = m.highSlot();
    if (lo == 0 || m.size() < MELD_MIN) return false;

    for (int i = 1; i <= MELD_SLOTS; ++i)
        if (m.wild[i] && !m.slot[i].valid()) return false;

    bool anyWild = false;
    for (int i = lo; i <= hi; ++i) {
        const Card& c = m.slot[i];
        if (!c.valid()) return false;  // hole inside the run
        if (m.wild[i]) {
            if (!isWild(c)) return false;
            anyWild = true;
            continue;
        }
        if (c.suit != m.suit) return false;
        if (c.rank == JOKER_RANK) return false;  // jokers are always wild
        // Slots follow ranks; an ace may also sit above the king in slot 14.
        if (c.rank != uint8_t(i) && !(c.rank == ACE && i == 14)) return false;
    }
    if (m.dirty != anyWild) return false;

    for (int i = lo; i <= hi; ++i)
        if (!claimCard(m.slot[i], seen)) return false;
    return true;
}

}  // namespace

bool Game::saveTo(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    f << kSaveMagic << '\n';
    f << "hand " << handNumber_ << '\n';
    f << "current " << current_ << '\n';
    f << "phase " << int(phase_) << '\n';
    f << "prompt " << int(prompt_) << '\n';
    f << "rng " << rng_ << '\n';
    f << "wentout " << lastWentOutSeat_ << ' ' << (lastWentOutByMeld_ ? 1 : 0) << '\n';
    f << "timer " << timer_ << '\n';
    f << "drawn " << int(lastDrawn_.id) << '\n';
    f << "stats " << handsPlayed_ << ' ' << wins_[0] << ' ' << wins_[1] << '\n';
    f << "ctrl " << int(controllers_[0]) << ' ' << int(controllers_[1]) << '\n';

    for (int seat = 0; seat < 2; ++seat) {
        const PlayerState& p = players_[seat];
        f << "name " << seat << ' ' << p.name << '\n';
        f << "player " << seat << ' ' << (p.tookMorto ? 1 : 0) << ' '
          << (p.wentOut ? 1 : 0) << ' ' << p.lastHandPoints << ' ' << p.total << ' '
          << p.hand.size();
        for (const Card& c : p.hand) f << ' ' << int(c.id);
        f << '\n';

        f << "melds " << seat << ' ' << p.melds.size() << '\n';
        for (const Meld& m : p.melds) {
            f << "meld " << seat << ' ' << int(m.suit) << ' ' << (m.dirty ? 1 : 0);
            for (int i = 1; i <= MELD_SLOTS; ++i) f << ' ' << int(m.slot[i].id);
            for (int i = 1; i <= MELD_SLOTS; ++i) f << ' ' << (m.wild[i] ? 1 : 0);
            f << '\n';
        }
    }

    auto dumpPile = [&f](const char* tag, const std::vector<Card>& pile) {
        f << tag << ' ' << pile.size();
        for (const Card& c : pile) f << ' ' << int(c.id);
        f << '\n';
    };
    dumpPile("monte", monte_);
    dumpPile("mesa", discard_);
    dumpPile("morto", morto_);

    f << "msg " << message_ << '\n';
    return bool(f);
}

bool Game::loadFrom(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    // Parse into a scratch copy so a corrupt file never halves a live match.
    Game g;
    std::string line;
    if (!std::getline(f, line)) return false;
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    if (line != kSaveMagic) return false;

    int meldCount[2] = {0, 0};
    int meldSeen[2] = {0, 0};
    bool statsSeen = false;

    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty()) continue;
        std::istringstream in(line);
        std::string tag;
        in >> tag;

        if (tag == "hand") {
            if (!(in >> g.handNumber_) || !atEnd(in)) return false;
        } else if (tag == "current") {
            if (!(in >> g.current_) || !atEnd(in)) return false;
        } else if (tag == "phase") {
            int v = 0;
            if (!(in >> v) || !atEnd(in)) return false;
            if (v < 0 || v > int(Phase::MatchOver)) return false;
            g.phase_ = Phase(v);
        } else if (tag == "prompt") {
            int v = 0;
            if (!(in >> v) || !atEnd(in)) return false;
            if (v < 0 || v > int(Prompt::NewHand)) return false;
            g.prompt_ = Prompt(v);
        } else if (tag == "rng") {
            unsigned long long v = 0;
            if (!(in >> v) || !atEnd(in)) return false;
            if (v > 0xFFFFFFFFull) return false;
            g.rng_ = uint32_t(v);
        } else if (tag == "wentout") {
            int seat = -1, byMeld = 0;
            if (!(in >> seat >> byMeld) || !atEnd(in)) return false;
            if (seat < -1 || seat > 1 || (byMeld != 0 && byMeld != 1)) return false;
            g.lastWentOutSeat_ = seat;
            g.lastWentOutByMeld_ = byMeld != 0;
        } else if (tag == "timer") {
            if (!(in >> g.timer_) || !atEnd(in)) return false;
            if (!std::isfinite(g.timer_) || g.timer_ < 0.0) return false;
        } else if (tag == "drawn") {
            int id = 0;
            if (!(in >> id) || !atEnd(in)) return false;
            if (!validId(id)) return false;
            g.lastDrawn_ = id ? makeCard(uint8_t(id)) : Card{};
        } else if (tag == "stats") {
            int played = 0, w0 = 0, w1 = 0;
            if (!(in >> played >> w0 >> w1) || !atEnd(in)) return false;
            if (played < 0 || w0 < 0 || w1 < 0) return false;
            if (w0 > played - w1) return false;  // w0 + w1 <= played
            g.handsPlayed_ = played;
            g.wins_[0] = w0;
            g.wins_[1] = w1;
            statsSeen = true;
        } else if (tag == "ctrl") {
            // Optional line: saves written before hot-seat have no controllers
            // and must keep loading as the historical {Human, AI} match.
            int c0 = 0, c1 = 0;
            if (!(in >> c0 >> c1) || !atEnd(in)) return false;
            if (c0 < 0 || c0 > int(Controller::Remote) || c1 < 0 ||
                c1 > int(Controller::Remote))
                return false;
            g.controllers_[0] = Controller(c0);
            g.controllers_[1] = Controller(c1);
        } else if (tag == "name") {
            int seat = -1;
            if (!(in >> seat)) return false;
            if (seat < 0 || seat > 1) return false;
            std::string rest;
            std::getline(in, rest);
            if (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());
            g.players_[seat].name = rest;
        } else if (tag == "player") {
            int seat = -1;
            if (!(in >> seat)) return false;
            if (seat < 0 || seat > 1) return false;
            int took = 0, out = 0, n = 0;
            if (!(in >> took >> out >> g.players_[seat].lastHandPoints >>
                  g.players_[seat].total >> n))
                return false;
            if ((took != 0 && took != 1) || (out != 0 && out != 1)) return false;
            if (n < 0 || n > DECK_SIZE) return false;
            g.players_[seat].tookMorto = took != 0;
            g.players_[seat].wentOut = out != 0;
            g.players_[seat].hand.clear();
            for (int i = 0; i < n; ++i) {
                int id = 0;
                if (!(in >> id) || !validId(id) || id == 0) return false;
                g.players_[seat].hand.push_back(makeCard(uint8_t(id)));
            }
            if (!atEnd(in)) return false;
        } else if (tag == "melds") {
            int seat = -1;
            if (!(in >> seat)) return false;
            if (seat < 0 || seat > 1) return false;
            if (!(in >> meldCount[seat]) || !atEnd(in)) return false;
            if (meldCount[seat] < 0 || meldCount[seat] > kMaxMelds) return false;
            g.players_[seat].melds.clear();
            meldSeen[seat] = 0;
        } else if (tag == "meld") {
            int seat = -1, suit = 0, dirty = 0;
            if (!(in >> seat >> suit >> dirty)) return false;
            if (seat < 0 || seat > 1 || suit < int(DIAMONDS) || suit > int(CLUBS) ||
                (dirty != 0 && dirty != 1))
                return false;
            Meld m;
            m.suit = uint8_t(suit);
            m.dirty = dirty != 0;
            for (int i = 1; i <= MELD_SLOTS; ++i) {
                int id = 0;
                if (!(in >> id) || !validId(id)) return false;
                m.slot[i] = id ? makeCard(uint8_t(id)) : Card{};
            }
            for (int i = 1; i <= MELD_SLOTS; ++i) {
                int w = 0;
                if (!(in >> w) || (w != 0 && w != 1)) return false;
                m.wild[i] = w != 0;
            }
            if (!atEnd(in)) return false;
            if (meldSeen[seat] >= meldCount[seat]) return false;
            ++meldSeen[seat];
            g.players_[seat].melds.push_back(m);
        } else if (tag == "monte" || tag == "mesa" || tag == "morto") {
            int n = 0;
            if (!(in >> n)) return false;
            if (n < 0 || n > DECK_SIZE) return false;
            std::vector<Card>& pile =
                (tag == "monte") ? g.monte_ : (tag == "mesa") ? g.discard_ : g.morto_;
            pile.clear();
            for (int i = 0; i < n; ++i) {
                int id = 0;
                if (!(in >> id) || id < 1 || id > DECK_SIZE) return false;
                pile.push_back(makeCard(uint8_t(id)));
            }
            if (!atEnd(in)) return false;
        } else if (tag == "msg") {
            std::string rest;
            std::getline(in, rest);
            if (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());
            g.message_ = rest;
        } else {
            return false;  // unknown tag: refuse the whole file
        }
    }

    if (g.current_ != 0 && g.current_ != 1) return false;
    if (meldSeen[0] != meldCount[0] || meldSeen[1] != meldCount[1]) return false;
    if (g.handNumber_ < 1) return false;
    // Older saves (before the F3 statistics were stored) have no `stats`
    // line; the number of completed hands is still derivable from the hand
    // counter, and the win split simply starts at zero.
    if (!statsSeen) g.handsPlayed_ = std::max(0, g.handNumber_ - 1);

    // No physical card may appear twice across hands, melds and piles.
    // lastDrawn_ is intentionally excluded: it is a pointer into a card that
    // may already have moved to a meld or to the table.
    std::array<int, DECK_SIZE + 1> seen{};
    for (int seat = 0; seat < 2; ++seat) {
        for (const Card& c : g.players_[seat].hand)
            if (!claimCard(c, seen)) return false;
        for (const Meld& m : g.players_[seat].melds)
            if (!validMeld(m, seen)) return false;
    }
    for (const Card& c : g.morto_)
        if (!claimCard(c, seen)) return false;
    for (const Card& c : g.monte_)
        if (!claimCard(c, seen)) return false;
    for (const Card& c : g.discard_)
        if (!claimCard(c, seen)) return false;

    *this = g;
    return true;
}

}  // namespace buraco
