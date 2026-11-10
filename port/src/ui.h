// 80x25 presentation of the game state.
#pragma once

#include "config.h"
#include "game.h"
#include "video.h"

#include <string>
#include <vector>

namespace ui {

constexpr int COLS = dos::TEXT_COLS;
constexpr int ROWS = dos::TEXT_ROWS;

constexpr uint8_t FIELD = 0x2F;     // bright white on green
constexpr uint8_t DIM = 0x2A;       // light green on green
constexpr uint8_t CARD = 0x70;      // black on light grey
constexpr uint8_t CARD_RED = 0x74;  // red on light grey
constexpr uint8_t CARD_SEL = 0x1F;  // white on blue
constexpr uint8_t BACK = 0x74;      // red on light grey (▒ hatch)
constexpr uint8_t WILD = 0x2E;      // yellow on green

constexpr int ROW_MSG = 0;
constexpr int ROW_RULE1 = 1;
constexpr int ROW_RULE2 = 7;
constexpr int ROW_RULE3 = 15;
constexpr int ROW_RULE4 = 24;
constexpr int COL_SPLIT = 39;
constexpr int ROW_PILE = 2;
constexpr int COL_MORTO = 1;
constexpr int COL_MESA = 11;
constexpr int ROW_LABEL = 16;
constexpr int ROW_CURSOR = 17;
constexpr int ROW_HAND = 19;
constexpr int ROW_MELD = 9;
constexpr int COL_MELD_L = 1;
constexpr int COL_MELD_R = 41;

struct UiState {
    // Screens shown before the actual game starts.  Headless/batch modes keep
    // the default `Game` so the historical scripts still start with ENTER.
    enum class Screen { Game, Intro, ContinuePrompt, NameEntry };
    Screen screen = Screen::Game;

    int cursor = 0;
    int meldScroll = 0;  // first meld row shown in the table panel
    std::vector<bool> selected;
    bool help = false;
    bool status = false;
    // Pending "grow an existing meld or lay a new one?" question.  While it is
    // set the selected cards are kept aside and only A/N/ESC are accepted.
    bool layChoice = false;
    int layTarget = -1;
    std::vector<buraco::Card> layCards;

    // Startup flow.
    std::string inputName;   // name being typed on the NameEntry screen
    std::string promptSave;  // save file offered on the ContinuePrompt screen
    std::string promptName;  // player name read from that save file
    std::string inputError;  // validation message shown under the name field
};

void draw(dos::Video& v, const buraco::Game& g, const UiState& ui);

// F9 options panel.  It covers the whole 80x25 screen and reads the live
// settings; `selectedRow` indexes options::Row.
void drawOptions(dos::Video& v, const config::Settings& settings, int selectedRow);

}  // namespace ui
