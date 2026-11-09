// Machine-readable state dumps and card-accounting checks used by the
// headless modes and by the regression tests.
#pragma once

#include "game.h"

#include <string>

namespace report {

// One line with the factors a replay cares about (hands, piles, melds, score).
std::string stateLine(const buraco::Game& g);

// Strict dump used by --savetest: also prints physical card ids and wildcard
// flags, so a save/load round-trip that swaps a duplicate card is visible.
std::string strictLine(const buraco::Game& g);

// Every one of the 108 cards must be in exactly one place.  Meld layouts must
// also be contiguous.  On failure `err` explains what went wrong.
bool checkInvariants(const buraco::Game& g, std::string& err);

}  // namespace report
