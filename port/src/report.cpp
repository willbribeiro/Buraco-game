#include "report.h"

#include <array>
#include <cstdio>

namespace report {

std::string stateLine(const buraco::Game& g) {
    char buf[512];
    std::string s;
    std::snprintf(buf, sizeof buf, "# state phase=%d cur=%d rng=%u hand=", int(g.phase()),
                  g.current(), unsigned(g.rng()));
    s += buf;
    for (const buraco::Card& c : g.player(0).hand) {
        std::snprintf(buf, sizeof buf, "%u/%u ", unsigned(c.suit), unsigned(c.rank));
        s += buf;
    }
    s += "ai=";
    for (const buraco::Card& c : g.player(1).hand) {
        std::snprintf(buf, sizeof buf, "%u/%u ", unsigned(c.suit), unsigned(c.rank));
        s += buf;
    }
    std::snprintf(buf, sizeof buf, "| morto=%d monte=%zu mesa=%zu", g.mortoRemaining(),
                  g.monte().size(), g.discardPile().size());
    s += buf;
    for (int seat = 0; seat < 2; ++seat) {
        std::snprintf(buf, sizeof buf, "| p%d melds:", seat);
        s += buf;
        for (const buraco::Meld& m : g.player(seat).melds) {
            if (m.empty()) continue;
            std::snprintf(buf, sizeof buf, " s%u n%d v%d[", unsigned(m.suit), m.size(), m.value());
            s += buf;
            for (const buraco::Card& c : m.cards()) {
                std::snprintf(buf, sizeof buf, "%u,", unsigned(c.id));
                s += buf;
            }
            s += "]";
        }
    }
    std::snprintf(buf, sizeof buf, "| pts %d x %d| h%d| stats %d/%d/%d\n",
                  g.player(0).total, g.player(1).total, g.handNumber(),
                  g.handsPlayed(), g.wins(0), g.wins(1));
    s += buf;
    return s;
}

std::string strictLine(const buraco::Game& g) {
    std::string s = stateLine(g);
    char buf[64];
    for (int seat = 0; seat < 2; ++seat) {
        std::snprintf(buf, sizeof buf, "H%d:", seat);
        s += buf;
        for (const buraco::Card& c : g.player(seat).hand) {
            std::snprintf(buf, sizeof buf, " %u", unsigned(c.id));
            s += buf;
        }
        for (const buraco::Meld& m : g.player(seat).melds) {
            std::snprintf(buf, sizeof buf, " [s%u d%d", unsigned(m.suit), m.dirty ? 1 : 0);
            s += buf;
            for (int i = 1; i <= buraco::MELD_SLOTS; ++i) {
                if (!m.slot[i].valid()) continue;
                std::snprintf(buf, sizeof buf, " %d=%u%s", i, unsigned(m.slot[i].id),
                              m.wild[i] ? "w" : "");
                s += buf;
            }
            s += "]";
        }
    }
    s += '\n';
    return s;
}

bool checkInvariants(const buraco::Game& g, std::string& err) {
    std::array<int, buraco::DECK_SIZE + 1> seen{};
    auto claim = [&](const buraco::Card& c, const char* where) {
        if (!c.valid() || c.id > buraco::DECK_SIZE) {
            err = std::string("invalid card id in ") + where;
            return false;
        }
        if (++seen[c.id] != 1) {
            err = std::string("duplicated card ") + std::to_string(int(c.id)) + " in " + where;
            return false;
        }
        return true;
    };
    for (int seat = 0; seat < 2; ++seat) {
        for (const buraco::Card& c : g.player(seat).hand)
            if (!claim(c, "hand")) return false;
        for (const buraco::Meld& m : g.player(seat).melds) {
            const int lo = m.lowSlot(), hi = m.highSlot();
            if (lo == 0) {
                err = "empty meld in list";
                return false;
            }
            for (int i = lo; i <= hi; ++i)
                if (!m.slot[i].valid()) {
                    err = "meld with a hole";
                    return false;
                }
            for (const buraco::Card& c : m.cards())
                if (!claim(c, "meld")) return false;
        }
    }
    for (const buraco::Card& c : g.morto()) if (!claim(c, "morto")) return false;
    for (const buraco::Card& c : g.monte()) if (!claim(c, "monte")) return false;
    for (const buraco::Card& c : g.discardPile()) if (!claim(c, "mesa")) return false;

    int total = 0;
    for (int id = 1; id <= buraco::DECK_SIZE; ++id) total += seen[id];
    if (total != buraco::DECK_SIZE) {
        err = "card count " + std::to_string(total) + " != 108";
        return false;
    }
    return true;
}

}  // namespace report
