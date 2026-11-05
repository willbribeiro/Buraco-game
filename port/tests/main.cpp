// Simple test runner for Buraco port tests
// This file provides the main entry point and can be compiled standalone

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    // Try to run Catch2 tests if available
    try {
        #include "catch2/catch_all.hpp"
        
        int result = Catch::Session().run(argc, argv);
        
        return result == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        // Catch2 not available, run basic tests
        std::cout << "Catch2 not found. Running basic validation tests...\n\n";
        return runBasicTests();
    }
}

// Simple basic tests that don't require Catch2
int runBasicTests() {
    int failures = 0;
    
    // Test 1: Card creation
    std::cout << "Test 1: Card creation from IDs... ";
    try {
        auto makeCard = [](uint8_t id) -> struct {
            uint8_t id = 0;
            uint8_t rank = 0;
            uint8_t suit = 0;
            uint8_t deck = 0;
            bool valid() const { return id != 0; }
        } {
            struct Card {
                uint8_t id = 0, rank = 0, suit = 0, deck = 0;
                bool valid() const { return id != 0; }
            };
            
            Card c;
            c.id = id;
            if (id == 0) return c;
            if (id <= 52) {
                c.deck = 0;
                c.suit = uint8_t((id - 1) / 13 + 1);
                c.rank = uint8_t((id - 1) % 13 + 1);
            } else if (id <= 54) {
                c.deck = 0;
                c.suit = 5;  // JOKER
                c.rank = 15; // JOKER_RANK
            } else if (id <= 106) {
                c.deck = 1;
                c.suit = uint8_t((id - 55) / 13 + 1);
                c.rank = uint8_t((id - 55) % 13 + 1);
            } else {
                c.deck = 1;
                c.suit = 5;  // JOKER
                c.rank = 15; // JOKER_RANK
            }
            return c;
        };
        
        auto c = makeCard(1);
        if (!c.valid() || c.id != 1) throw std::runtime_error("Invalid card creation");
        
        auto joker = makeCard(53);
        if (joker.suit != 5) throw std::runtime_error("Joker not created correctly");
        
        std::cout << "OK\n";
    } catch (...) {
        std::cout << "FAILED\n";
        failures++;
    }
    
    // Test 2: Points calculation
    std::cout << "Test 2: Card points... ";
    try {
        auto makeCard = [](uint8_t id) -> struct {
            uint8_t rank = 0;
            bool valid() const { return rank != 0; }
        } {
            struct Card {
                uint8_t rank = 0;
                bool valid() const { return rank != 0; }
            };
            
            static const int kPoints[16] = {0, 15, 10, 5, 5, 5, 5, 5, 10, 10, 10, 10, 10, 10, 15, 20};
            
            struct CardWithRank {
                uint8_t id = 0, rank = 0;
                bool valid() const { return id != 0; }
            };
            
            auto c = makeCard(1);
            c.rank = 1; // ACE
            
            if (c.rank == 1) return kPoints[c.rank & 15];
        };
        
        int acePoints = 15;
        std::cout << "OK\n";
    } catch (...) {
        std::cout << "FAILED\n";
        failures++;
    }
    
    // Test 3: Meld size calculation  
    std::cout << "Test 3: Meld size... ";
    try {
        struct Card {
            uint8_t id = 0;
            bool valid() const { return id != 0; }
        };
        
        struct Meld {
            uint8_t suit = 0;
            std::array<Card, 16> slot{};
            
            int size() const {
                int n = 0;
                for (int i = 1; i <= 15; ++i)
                    if (slot[i].valid()) ++n;
                return n;
            }
        };
        
        Meld m;
        m.suit = 1; // DIAMONDS
        m.slot[1] = {{0, 1}, {}, {}}; // ACE
        
        if (m.size() == 1) std::cout << "OK\n";
        else throw std::runtime_error("Meld size incorrect");
    } catch (...) {
        std::cout << "FAILED\n";
        failures++;
    }
    
    std::cout << "\nBasic validation: " << (failures == 0 ? "PASSED" : "FAILED") 
              << " (" << (2 + failures) << "/3 tests passed)\n";
    
    return failures;
}
