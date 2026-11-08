// Game state machine for the Buraco port.
//
// Reconstructed turn flow (FUN_1000_662c):
//   phase 0/1 -> draw      (FUN_1000_3f33: monte or discard pile)
//   phase 2   -> play      (FUN_1000_557f: lay melds)
//   phase 3   -> discard   (FUN_1000_5f56)
//   phase 4   -> switch player
//   ends when 0xBF0 (someone went out) or 0xBF1 (quit), then scores with
//   FUN_1000_61be.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "cards.h"

namespace buraco {

enum class Phase {
    Draw,       // current player must take a card
    Play,       // current player may lay melds
    Discard,    // current player must discard one card
    HandOver,   // scores shown, waiting for a key
    MatchOver,
};

enum class Prompt { None, TakeDiscard, NewHand };

struct PlayerState {
    std::string name;
    std::vector<Card> hand;
    std::vector<Meld> melds;   // up to 15 slots, any mix of suits
    bool tookMorto = false;
    bool wentOut = false;
    int lastHandPoints = 0;    // points scored in the hand that just ended
    int total = 0;             // accumulated match score
};

class Game {
public:
    Game();

    void newMatch(const std::string& playerName, uint32_t seed);
    void newHand();

    // ---- state -----------------------------------------------------------
    const PlayerState& player(int seat) const { return players_[seat]; }
    PlayerState& player(int seat) { return players_[seat]; }
    int current() const { return current_; }
    Phase phase() const { return phase_; }
    Prompt prompt() const { return prompt_; }
    const std::string& message() const { return message_; }
    const std::vector<Card>& monte() const { return monte_; }
    const std::vector<Card>& discardPile() const { return discard_; }
    const std::vector<Card>& morto() const { return morto_; }
    int mortoRemaining() const { return int(morto_.size()); }
    const Card& lastDrawn() const { return lastDrawn_; }
    bool matchOver() const { return phase_ == Phase::MatchOver; }
    int handNumber() const { return handNumber_; }
    int lastWentOutSeat() const { return lastWentOutSeat_; }
    uint32_t rng() const { return rng_; }

    // Match statistics shown by F3.  `handsPlayed` counts the hands that were
    // already scored (the hand in progress is not counted yet); `wins` counts
    // who emptied their hand in each of those hands.
    int handsPlayed() const { return handsPlayed_; }
    int wins(int seat) const { return wins_[seat]; }
    int meldCount(int seat) const {
        int n = 0;
        for (const Meld& m : players_[seat].melds)
            if (!m.empty()) ++n;
        return n;
    }

    void setMessage(const std::string& m) { message_ = m; }

    // ---- persistence (F2 / F5) -------------------------------------------
    // Saves/restores the complete mutable state, including the RNG, so a
    // restored match continues exactly where it stopped.  The file is a plain
    // token stream (see game.cpp) and is validated on load.
    bool saveTo(const std::string& path) const;
    bool loadFrom(const std::string& path);

    // ---- actions ---------------------------------------------------------
    // Draw phase.  `fromTable` takes the top of the discard pile.
    bool draw(bool fromTable);
    bool canTakeDiscard() const;

    // Play phase: lay the given cards as a new meld or add them to an existing
    // one.  A new meld needs at least three cards, but growing an existing
    // meld accepts any number of cards (even one).  Returns an empty string on
    // success, otherwise a user-facing reason.
    //
    // `target` selects what to do when both are possible:
    //   -1       auto (extend the first compatible meld, else create a new one)
    //   >= 0     grow the existing meld with that index
    //   kNewMeld create a second meld even when another of the suit exists
    std::string layMeld(const std::vector<Card>& cards, int target = -1);

    // What the selected cards can do right now, used by the UI to offer the
    // "grow or lay a new meld" choice.  `targets` lists the melds that accept
    // the cards; `canCreate` says a new meld can be laid.
    struct LayOptions {
        bool isMeld = false;         // the cards form a legal meld on their own
        bool canCreate = false;      // there is room for another meld
        std::vector<int> targets;    // indices into player(current).melds
    };
    LayOptions layOptions(const std::vector<Card>& cards) const;

    // Discard phase.
    bool discardCard(const Card& card);

    // Leave the play phase and move on to discarding.
    void requestDiscard();

    // Prompt answers.
    void answerPrompt(bool yes);

    // Advance timers / AI.  Call once per frame.
    void update(double dt);

    // Plays the whole current phase for whoever is to move (draw, melds,
    // discard).  Used by the AI and by the headless self-play mode.
    void autoStep();

    // Re-emits the prompt for the current phase (used after a demo replay).
    void announceTurn();

    // Hand-over bookkeeping.
    void continueAfterHand();

    int targetScore() const { return kTargetScore; }

    static constexpr int kNewMeld = -2;   // layMeld target: force a new meld

private:
    static constexpr int kTargetScore = 3000;

    // How many melds a player may have on the table.  The original keeps 15
    // meld slots per player (FUN_1000_4926 scans slots 1..15), so there is no
    // "four melds" rule and any number of runs may share a suit.
    static constexpr int kMaxMelds = 15;

    void beginTurn();
    void nextTurn();
    void endHand(bool playerWentOut, bool playerWonByMeld);
    // Called whenever the current player's hand became empty.  Takes the morto
    // when it is still available, otherwise scores the hand (batida).
    void afterEmptyHand(bool byMeld);
    int scoreHand(int seat, bool wentOut, int bonus) const;

    void aiTurn();
    bool aiLayMelds();
    void aiDiscard();
    std::vector<Card> aiFindMeld() const;
    bool canUseTableCard(const Card& top) const;

    void setPhase(Phase p);
    void info(const std::string& m);

    PlayerState players_[2];
    std::vector<Card> monte_;
    std::vector<Card> discard_;
    std::vector<Card> morto_;
    std::string message_;
    Phase phase_ = Phase::Draw;
    Prompt prompt_ = Prompt::None;
    int current_ = 0;
    int handNumber_ = 0;
    int handsPlayed_ = 0;
    int wins_[2] = {0, 0};
    uint32_t rng_ = 1;
    double timer_ = 0.0;
    bool lastWentOutByMeld_ = false;
    int lastWentOutSeat_ = -1;
    Card lastDrawn_;   // card taken this turn (table or monte), used by the AI

    uint32_t rnd();
    void shuffle(std::vector<Card>& deck);
};

}  // namespace buraco
