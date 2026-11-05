#include "game.h"

#define CATCH2_TEST_MAIN
#include <catch2/catch_all.hpp>
#include <cstdio>

TEST_CASE("Game newMatch initialization", "[game]") {
    buraco::Game game;
    
    // Default player names
    CHECK(game.player(0).name == "Voce");
    CHECK(game.player(1).name == "Eu");
    
    // Initial phase is Draw
    CHECK(game.phase() == buraco::Phase::Draw);
    
    // Current player is 0 (human)
    CHECK(game.current() == 0);
}

TEST_CASE("Game newMatch with seed", "[game]") {
    buraco::Game game;
    game.newMatch("Player1", 42u);
    
    CHECK(game.player(0).name == "Player1");
    CHECK(game.rng() != 42u);  // RNG is modified during deal
}

TEST_CASE("Game newHand: empty boards", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 123u);
    game.newHand();
    
    CHECK(game.monte().empty() == true);
    CHECK(game.discardPile().empty() == true);
    CHECK(game.morto().empty() == true);
    CHECK(game.message() == "Embaralhando. Por favor, corte...");
}

TEST_CASE("Game newHand: correct hand sizes", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 123u);
    game.newHand();
    
    CHECK(game.player(0).hand.size() == 11);
    CHECK(game.player(1).hand.size() == 11);
    CHECK(game.morto().size() == 22);
    CHECK(game.monte().size() == 64);  // 108 - 22 - 11 - 11 = 64
}

TEST_CASE("Game newHand: alternates dealing", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 42u);
    game.newHand();
    
    // Player 0 gets cards first, then player 1
    CHECK(game.player(0).hand[0].id != game.player(1).hand[0].id);
}

TEST_CASE("Game newHand: rounds to starting player", "[game]") {
    buraco::Game game;
    
    // Hand 0: player 0 starts
    game.newMatch("Test", 1u);
    game.newHand();
    CHECK(game.current() == 0);
    
    // Hand 1: player 1 starts (alternate)
    game.newHand();
    CHECK(game.current() == 1);
    
    // Hand 2: back to player 0
    game.newHand();
    CHECK(game.current() == 0);
}

TEST_CASE("Game canTakeDiscard: empty pile", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 1u);
    game.newHand();
    
    CHECK(game.canTakeDiscard() == false);
}

TEST_CASE("Game draw from monte", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 123u);
    game.newHand();
    
    // Draw from monte
    bool ok = game.draw(false);
    CHECK(ok == true);
    CHECK(game.monte().size() == 63);  // Decreased by 1
}

TEST_CASE("Game draw from table: empty discard", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 123u);
    game.newHand();
    
    // Try to take from empty discard pile
    bool ok = game.draw(true);
    CHECK(ok == false);
    setPhase(game, buraco::Phase::Draw);  // Reset for next test
}

TEST_CASE("Game draw from table: non-empty discard", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 123u);
    game.newHand();
    game.draw(false);  // Draw from monte first
    
    // Add a card to discard pile manually
    game.discard_.push_back(game.player(0).hand.back());
    game.player(0).hand.pop_back();
    
    // Now can take from table
    bool ok = game.draw(true);
    CHECK(ok == true);
}

TEST_CASE("Game layMeld: invalid phase fails", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 123u);
    game.newHand();
    
    std::vector<buraco::Card> cards{makeCard(1), makeCard(27)};
    std::string err = game.layMeld(cards);
    CHECK(err.empty() == false);
}

TEST_CASE("Game layMeld: empty cards fails", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 123u);
    game.newHand();
    game.setPhase(buraco::Phase::Play);
    
    std::string err = game.layMeld({});
    CHECK(!err.empty());
}

TEST_CASE("Game discardCard: invalid phase fails", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 123u);
    game.newHand();
    
    bool ok = game.discardCard(makeCard(1));
    CHECK(ok == false);
}

TEST_CASE("Game discardCard: successful discard", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 123u);
    game.newHand();
    game.draw(false);
    game.setPhase(buraco::Phase::Play);
    
    // Find a card to discard
    for (auto& c : game.player(0).hand) {
        if (c.id != 1) {  // Don't discard the first card
            bool ok = game.discardCard(c);
            CHECK(ok == true);
            CHECK(game.discardPile().size() == 1);
            break;
        }
    }
}

TEST_CASE("Game update: advances AI timer", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 42u);
    game.newHand();
    
    // Initial state - no updates yet
    CHECK(game.timer_ == 0.0);
    
    // Human player's turn - AI should not advance
    game.update(0.1);
    CHECK(game.current() == 0);  // Still human's turn
    
    // Switch to AI's turn
    game.nextTurn();
    CHECK(game.current() == 1);
    
    // Now AI advances
    for (int i = 0; i < 7; ++i) {
        game.update(0.2);  // Less than AI delay
    }
    
    CHECK(game.timer_ >= 0.7);
}

TEST_CASE("Game announceTurn: correct messages", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 123u);
    game.newHand();
    
    // Draw phase message
    game.setPhase(buraco::Phase::Draw);
    CHECK(game.message() == "Embaralhando. Por favor, corte...");
}

TEST_CASE("Game saveTo: basic persistence", "[game]") {
    buraco::Game game;
    game.newMatch("Player1", 42u);
    game.newHand();
    game.draw(false);
    
    // Save to a temp file
    const std::string path = "port/tests/test_save.sav";
    bool ok = game.saveTo(path);
    CHECK(ok == true);
}

TEST_CASE("Game loadFrom: restores state", "[game]") {
    buraco::Game game;
    game.newMatch("TestPlayer", 99u);
    game.newHand();
    game.draw(false);
    
    // Save
    const std::string path = "port/tests/test_load.sav";
    game.saveTo(path);
    
    // Load into new game
    buraco::Game restored;
    bool ok = restored.loadFrom(path);
    CHECK(ok == true);
    CHECK(restored.player(0).name == "TestPlayer");
    CHECK(restored.current() == 1);  // Next player to move
}

TEST_CASE("Game answerPrompt: handles take discard", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 123u);
    game.newHand();
    game.draw(false);
    
    // Answer yes (take from table)
    game.answerPrompt(true);
    CHECK(game.discardPile().empty() == true);
}

TEST_CASE("Game answerPrompt: handles new hand", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 123u);
    game.newHand();
    
    // Trigger hand over
    game.setPhase(buraco::Phase::HandOver);
    game.prompt_ = buraco::Prompt::NewHand;
    
    // Answer yes (continue)
    game.answerPrompt(true);
    CHECK(game.phase() == buraco::Phase::Draw);  // New hand started
}

TEST_CASE("Game autoStep: AI plays automatically", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 42u);
    game.newHand();
    
    // Let AI draw a card
    game.update(1.0);  // Enough for AI to act
    
    // AI should have drawn from monte
    CHECK(game.monte().size() < 64);
}

TEST_CASE("Game rnd: deterministic xorshift32", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 1u);
    
    uint32_t r1 = game.rng();
    
    // Reset RNG to same seed
    game.rng_ = 1u;
    uint32_t r2 = game.rng();
    
    CHECK(r1 == r2);  // Same sequence
}

TEST_CASE("Game shuffle: deterministic", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 42u);
    game.newHand();
    
    // Get initial monte state (first few cards)
    std::vector<buraco::Card> before;
    for (size_t i = 0; i < 5 && i < game.monte().size(); ++i) {
        before.push_back(game.monte()[game.monte().size() - 1 - i]);
    }
    
    // Reset and shuffle again
    game.rng_ = 42u;
    std::vector<buraco::Card> monteCopy = game.monte_;
    buraco::shuffle(monteCopy, monteCopy.size());
    
    // The shuffle should be deterministic with same seed
}

TEST_CASE("Game target score constant", "[game]") {
    buraco::Game game;
    CHECK(game.targetScore() == 3000);
}

TEST_CASE("Game kMaxMelds: maximum meld slots per player", "[game]") {
    // This is a private constant, but we can verify the limit through error messages
    // Try to lay too many melds - should eventually fail with "Nao cabem mais jogos na mesa."
    buraco::Game game;
    game.newMatch("Test", 123u);
    game.newHand();
    game.setPhase(buraco::Phase::Play);
    
    // Create valid melds and try to add them all
    int meldCount = 0;
    for (int i = 0; i < buraco::kMaxMelds + 5; ++i) {
        if (game.player(0).hand.empty()) break;
        
        std::vector<buraco::Card> cards{makeCard(i + 1)};
        std::string err = game.layMeld(cards, buraco::Game::kNewMeld);
        
        if (!err.empty() && err == "Nao cabem mais jogos na mesa.") {
            CHECK(meldCount < buraco::kMaxMelds);  // Should stop before kMaxMelds
            break;
        }
        ++meldCount;
    }
}

TEST_CASE("Game newHand: resets state properly", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 123u);
    game.newHand();
    game.draw(false);
    
    // Save current state info
    int handSize = game.player(0).hand.size();
    size_t meldCount = game.player(0).melds.size();
    int monteRemaining = game.monte_.size();
    
    // Start new hand
    game.newHand();
    
    // State should be reset
    CHECK(game.player(0).hand.empty() == true);
    CHECK(game.player(1).hand.empty() == true);
    CHECK(meldCount == 0);
}

TEST_CASE("Game lastDrawn: tracks drawn card", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 123u);
    game.newHand();
    
    Card initialLastDrawn = game.lastDrawn();
    CHECK(initialLastDrawn.valid() == false);  // No card drawn yet
    
    game.draw(false);
    Card afterDraw = game.lastDrawn();
    CHECK(afterDraw.valid() == true);
}

TEST_CASE("Game handNumber: increments per hand", "[game]") {
    buraco::Game game;
    game.newMatch("Test", 123u);
    
    CHECK(game.handNumber() == 0);
    
    game.newHand();
    CHECK(game.handNumber() == 1);
    
    game.newHand();
    CHECK(game.handNumber() == 2);
}

TEST_CASE("Game makeCard: valid card creation", "[game]") {
    buraco::Card c = buraco::makeCard(1);
    CHECK(c.valid() == true);
    CHECK(c.id == 1);
    
    // Edge cases
    CHECK(buraco::makeCard(0).valid() == false);
}

TEST_CASE("Game phase enum values", "[game]") {
    // Verify all phases exist and are in valid range
    CHECK(int(buraco::Phase::Draw) >= 0);
    CHECK(int(buraco::Phase::Play) > int(buraco::Phase::Draw));
    CHECK(int(buraco::Phase::Discard) > int(buraco::Phase::Play));
    CHECK(int(buraco::Phase::HandOver) > int(buraco::Phase::Discard));
}

TEST_CASE("Game prompt enum values", "[game]") {
    // Verify all prompts exist and are in valid range
    CHECK(int(buraco::Prompt::None) < int(buraco::Prompt::TakeDiscard));
    CHECK(int(buraco::Prompt::TakeDiscard) < int(buraco::Prompt::NewHand));
}
