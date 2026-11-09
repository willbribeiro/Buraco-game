#include "game.h"

#include <algorithm>
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
//   name <seat> <text to end of line>
//   player <seat> <tookMorto> <wentOut> <lastHandPoints> <total> <n> <ids...>
//   melds <seat> <count>
//   meld <seat> <suit> <dirty> <15 ids> <15 wild flags>
//   monte <n> <ids...>   mesa <n> <ids...>   morto <n> <ids...>
//   msg <text to end of line>

namespace {

constexpr const char* kSaveMagic = "BURACO-SAVE 1";

bool validId(int id) { return id == 0 || (id >= 1 && id <= DECK_SIZE); }

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
            for (int i = 1; i <= 15; ++i) f << ' ' << int(m.slot[i].id);
            for (int i = 1; i <= 15; ++i) f << ' ' << (m.wild[i] ? 1 : 0);
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
            in >> g.handNumber_;
        } else if (tag == "current") {
            in >> g.current_;
        } else if (tag == "phase") {
            int v = 0;
            in >> v;
            if (v < 0 || v > int(Phase::MatchOver)) return false;
            g.phase_ = Phase(v);
        } else if (tag == "prompt") {
            int v = 0;
            in >> v;
            if (v < 0 || v > int(Prompt::NewHand)) return false;
            g.prompt_ = Prompt(v);
        } else if (tag == "rng") {
            in >> g.rng_;
        } else if (tag == "wentout") {
            int byMeld = 0;
            in >> g.lastWentOutSeat_ >> byMeld;
            g.lastWentOutByMeld_ = byMeld != 0;
        } else if (tag == "timer") {
            in >> g.timer_;
        } else if (tag == "drawn") {
            int id = 0;
            in >> id;
            if (!validId(id)) return false;
            g.lastDrawn_ = id ? makeCard(uint8_t(id)) : Card{};
        } else if (tag == "stats") {
            int played = 0, w0 = 0, w1 = 0;
            if (!(in >> played >> w0 >> w1)) return false;
            if (played < 0 || w0 < 0 || w1 < 0) return false;
            g.handsPlayed_ = played;
            g.wins_[0] = w0;
            g.wins_[1] = w1;
            statsSeen = true;
        } else if (tag == "name") {
            int seat = -1;
            in >> seat;
            if (seat < 0 || seat > 1) return false;
            std::string rest;
            std::getline(in, rest);
            if (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());
            g.players_[seat].name = rest;
        } else if (tag == "player") {
            int seat = -1;
            in >> seat;
            if (seat < 0 || seat > 1) return false;
            int took = 0, out = 0, n = 0;
            in >> took >> out >> g.players_[seat].lastHandPoints >>
                g.players_[seat].total >> n;
            if (n < 0 || n > DECK_SIZE) return false;
            g.players_[seat].tookMorto = took != 0;
            g.players_[seat].wentOut = out != 0;
            g.players_[seat].hand.clear();
            for (int i = 0; i < n; ++i) {
                int id = 0;
                if (!(in >> id) || !validId(id) || id == 0) return false;
                g.players_[seat].hand.push_back(makeCard(uint8_t(id)));
            }
        } else if (tag == "melds") {
            int seat = -1;
            in >> seat;
            if (seat < 0 || seat > 1) return false;
            in >> meldCount[seat];
            if (meldCount[seat] < 0 || meldCount[seat] > kMaxMelds) return false;
            g.players_[seat].melds.clear();
        } else if (tag == "meld") {
            int seat = -1, suit = 0, dirty = 0;
            in >> seat >> suit >> dirty;
            if (seat < 0 || seat > 1 || suit < 0 || suit > JOKER || dirty < 0 || dirty > 1)
                return false;
            Meld m;
            m.suit = uint8_t(suit);
            m.dirty = dirty != 0;
            for (int i = 1; i <= 15; ++i) {
                int id = 0;
                if (!(in >> id) || !validId(id)) return false;
                m.slot[i] = id ? makeCard(uint8_t(id)) : Card{};
            }
            for (int i = 1; i <= 15; ++i) {
                int w = 0;
                if (!(in >> w)) return false;
                m.wild[i] = w != 0;
            }
            if (meldSeen[seat] >= meldCount[seat]) return false;
            ++meldSeen[seat];
            g.players_[seat].melds.push_back(m);
        } else if (tag == "monte" || tag == "mesa" || tag == "morto") {
            int n = 0;
            in >> n;
            if (n < 0 || n > DECK_SIZE) return false;
            std::vector<Card>& pile =
                (tag == "monte") ? g.monte_ : (tag == "mesa") ? g.discard_ : g.morto_;
            pile.clear();
            for (int i = 0; i < n; ++i) {
                int id = 0;
                if (!(in >> id) || id < 1 || id > DECK_SIZE) return false;
                pile.push_back(makeCard(uint8_t(id)));
            }
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

    *this = g;
    return true;
}

}  // namespace buraco
