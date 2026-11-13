// Wire protocol for the network foundation.
//
// The game stays fully deterministic: both peers start the same match from the
// same seed and apply the exact same action stream, so a small action message
// keeps the two Game instances in lockstep.  The host remains authoritative:
// it validates the remote seat's action before applying and relaying it.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "cards.h"
#include "game.h"

namespace buraco::net {

inline constexpr int kProtocolVersion = 1;
inline constexpr std::size_t kMaxNameLength = 40;

enum class ActionKind {
    DrawStock,
    DrawTable,
    LayMeld,
    Discard,
    RequestDiscard,
    PromptYes,
    PromptNo,
    Continue,
};

struct Action {
    ActionKind kind = ActionKind::DrawStock;
    std::vector<Card> cards;  // LayMeld
    int target = -1;          // LayMeld: -2 new meld, -1 auto, >= 0 grow
    Card card;                // Discard
};

// `BURACO-NET 1 HELLO <name>`
std::string makeHello(const std::string& name);
bool parseHello(const std::string& message, std::string& name,
                std::string* err = nullptr);

// `BURACO-NET 1 SEAT <0|1>` (host -> client)
std::string makeSeat(int seat);
bool parseSeat(const std::string& message, int& seat,
               std::string* err = nullptr);

// `BURACO-NET 1 START <seed>` (host -> client)
std::string makeStart(std::uint32_t seed);
bool parseStart(const std::string& message, std::uint32_t& seed,
                std::string* err = nullptr);

// `BURACO-NET 1 FP <16 hex digits>` (host -> client)
std::string makeFingerprint(const std::string& text);
bool parseFingerprint(const std::string& message, std::string& text,
                      std::string* err = nullptr);

// `BURACO-NET 1 ERR <text>` (either direction, one line)
std::string makeError(const std::string& text);
bool parseError(const std::string& message, std::string& text,
                std::string* err = nullptr);

// Tag of a well-formed envelope ("HELLO", "ACT", ...), or "" when invalid.
std::string tagOf(const std::string& message);

// `BURACO-NET 1 ACT <action>`
std::string makeAction(const Action& action);
bool parseAction(const std::string& message, Action& action,
                 std::string* err = nullptr);

// Applies the action to the seat that is currently to move.  Returns false and
// fills `err` when the game refused it; the caller (host) must not relay a
// refused action.
bool apply(Game& game, const Action& action, std::string* err = nullptr);

// Stable 64-bit fingerprint of the rule-relevant state (cards, piles, melds,
// phase, current seat, RNG and score).  Peers compare it after each action to
// detect a desynchronised match.
std::uint64_t fingerprint(const Game& game);
std::string fingerprintText(const Game& game);

}  // namespace buraco::net
