// Shared helpers for the Buraco unit tests.
#pragma once

#include "cards.h"
#include "game.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace testutil {

inline std::string tempSavePath(const std::string& name) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "buraco_tests";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return (dir / name).string();
}

inline bool writeText(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << text;
    return bool(out);
}

// Builds a save file with only the fields a test needs.  `loadFrom` treats
// absent tags as defaults, but `hand` is mandatory (it must be >= 1).
inline std::string saveHeader(int phase = 2, int current = 0) {
    std::ostringstream out;
    out << "BURACO-SAVE 1\n";
    out << "hand 1\n";
    out << "current " << current << "\n";
    out << "phase " << phase << "\n";
    return out.str();
}

inline std::string playerLine(int seat, const std::vector<uint8_t>& hand, bool tookMorto = true,
                              int lastHandPoints = 0, int total = 0) {
    std::ostringstream out;
    out << "player " << seat << ' ' << (tookMorto ? 1 : 0) << " 0 " << lastHandPoints << ' '
        << total << ' ' << hand.size();
    for (uint8_t id : hand) out << ' ' << int(id);
    out << '\n';
    return out.str();
}

// Writes a meld that occupies the given 1-based slots.  Slots not listed stay
// empty; `wild` marks slots filled by a wildcard.
inline std::string meldLine(int seat, uint8_t suit,
                            const std::vector<std::pair<int, uint8_t>>& slots,
                            const std::vector<int>& wild = {}, bool dirty = false) {
    std::vector<uint8_t> ids(buraco::MELD_SLOTS + 1, 0);
    std::vector<int> flags(buraco::MELD_SLOTS + 1, 0);
    for (const auto& [slot, id] : slots) {
        if (slot < 1 || slot > buraco::MELD_SLOTS) continue;
        ids[slot] = id;
    }
    for (int slot : wild) {
        if (slot < 1 || slot > buraco::MELD_SLOTS) continue;
        flags[slot] = 1;
    }
    std::ostringstream out;
    out << "meld " << seat << ' ' << int(suit) << ' ' << (dirty ? 1 : 0);
    for (int i = 1; i <= buraco::MELD_SLOTS; ++i) out << ' ' << int(ids[i]);
    for (int i = 1; i <= buraco::MELD_SLOTS; ++i) out << ' ' << flags[i];
    out << '\n';
    return out.str();
}

inline std::string pileLine(const char* tag, const std::vector<uint8_t>& ids) {
    std::ostringstream out;
    out << tag << ' ' << ids.size();
    for (uint8_t id : ids) out << ' ' << int(id);
    out << '\n';
    return out.str();
}

// Loads a game from an in-memory save description.  Returns false when the
// save is rejected.
inline bool loadText(buraco::Game& game, const std::string& text,
                     const std::string& name = "state.sav") {
    const std::string path = tempSavePath(name);
    if (!writeText(path, text)) return false;
    return game.loadFrom(path);
}

// Full public state, used to prove that save/load and refactors do not change
// the game.  Mirrors Game::saveTo but reads only public accessors.
inline std::string snapshot(const buraco::Game& g) {
    std::ostringstream out;
    out << "phase=" << int(g.phase()) << " current=" << g.current()
        << " prompt=" << int(g.prompt()) << " hand=" << g.handNumber()
        << " rng=" << g.rng() << " wentout=" << g.lastWentOutSeat()
        << " stats=" << g.handsPlayed() << '/' << g.wins(0) << '/' << g.wins(1)
        << " drawn=" << int(g.lastDrawn().id) << " msg=" << g.message() << '\n';
    for (int seat = 0; seat < 2; ++seat) {
        const buraco::PlayerState& p = g.player(seat);
        out << seat << " name=" << p.name << " took=" << p.tookMorto
            << " out=" << p.wentOut << " last=" << p.lastHandPoints
            << " total=" << p.total << " hand=";
        for (const buraco::Card& c : p.hand) out << int(c.id) << ',';
        out << " melds=";
        for (const buraco::Meld& m : p.melds) {
            out << '[' << int(m.suit) << ',' << int(m.dirty);
            for (int i = 1; i <= buraco::MELD_SLOTS; ++i) {
                out << ',' << int(m.slot[i].id) << ':' << int(m.wild[i]);
            }
            out << ']';
        }
        out << '\n';
    }
    out << "monte=";
    for (const buraco::Card& c : g.monte()) out << int(c.id) << ',';
    out << " mesa=";
    for (const buraco::Card& c : g.discardPile()) out << int(c.id) << ',';
    out << " morto=";
    for (const buraco::Card& c : g.morto()) out << int(c.id) << ',';
    out << '\n';
    return out.str();
}

}  // namespace testutil
