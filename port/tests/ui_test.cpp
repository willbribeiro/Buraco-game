// Screen rendering: geometry, screens, and a golden hash of a deterministic
// self-play game (the same 400-step replay used by --auto).
#include "ui.h"

#include "game.h"
#include "video.h"

#include "test_framework.hpp"
#include "test_helpers.h"

#include <cstdint>
#include <string>

using namespace buraco;

namespace {

std::string rowText(const dos::Video& v, int row) {
    std::string s;
    for (int c = 0; c < ui::COLS; ++c) s += char(v.text[(row * ui::COLS + c) * 2]);
    return s;
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

// FNV-1a over the 25 visible character rows, exactly what the --auto baseline
// prints.  The same hash can be computed from a captured auto400.txt dump.
uint64_t screenHash(const dos::Video& v) {
    uint64_t h = 1469598103934665603ull;
    for (int r = 0; r < ui::ROWS; ++r) {
        for (int c = 0; c < ui::COLS; ++c) {
            h ^= static_cast<unsigned char>(v.text[(r * ui::COLS + c) * 2]);
            h *= 1099511628211ull;
        }
        h ^= '\n';
        h *= 1099511628211ull;
    }
    return h;
}

void playAuto(buraco::Game& g, int steps) {
    for (int i = 0; i < steps;) {
        if (g.matchOver()) break;
        if (g.phase() == Phase::HandOver) {
            g.continueAfterHand();
            continue;
        }
        g.autoStep();
        ++i;
    }
}

}  // namespace

TEST_CASE("the deterministic 400-step game renders the same screen as before") {
    Game g;
    g.newMatch("JOGADOR", 1234u);
    playAuto(g, 400);

    dos::Video v;
    ui::UiState state;
    state.selected.assign(g.player(0).hand.size(), false);
    ui::draw(v, g, state);

    // Golden value captured from the pre-split --auto 400 --seed 1234 output.
    CHECK_EQ(screenHash(v), uint64_t(6006766675085491041ull));
}

TEST_CASE("the game screen has the historical 80x25 layout") {
    Game g;
    g.newMatch("Ana", 7u);
    dos::Video v;
    ui::UiState state;
    ui::draw(v, g, state);

    CHECK_EQ(int(v.text.size()), ui::COLS * ui::ROWS * 2);
    CHECK_EQ(int(v.text[(ui::ROW_MSG * ui::COLS) * 2]), int('C'));  // "Compre uma carta..."
    CHECK_EQ(int(v.text[(ui::ROW_RULE1 * ui::COLS) * 2]), 0xC4);    // ─
    CHECK_EQ(int(v.text[(ui::ROW_RULE4 * ui::COLS) * 2]), 0xC4);
    CHECK_EQ(int(v.text[(ui::ROW_RULE2 + 1) * ui::COLS * 2 + ui::COL_SPLIT * 2]), 0xBA);
    CHECK(rowText(v, ui::ROW_LABEL).find("Sua mao.") != std::string::npos);
    CHECK(rowText(v, ui::ROW_RULE2 + 1).find("SEUS JOGOS") != std::string::npos);
    CHECK(rowText(v, ui::ROW_RULE2 + 1).find("MEUS JOGOS") != std::string::npos);
}

TEST_CASE("the intro screen is the framed help text") {
    dos::Video v;
    ui::UiState state;
    state.screen = ui::UiState::Screen::Intro;
    Game g;
    ui::draw(v, g, state);

    CHECK(startsWith(rowText(v, 0), "BURACO - ajuda"));
    CHECK_EQ(int(v.text[(1 * ui::COLS + 1) * 2]), 0xC9);  // ╔
    CHECK_EQ(int(v.text[(23 * ui::COLS + 78) * 2]), 0xBC); // ╝
    CHECK(rowText(v, 2).find("Vamos jogar buraco ?") != std::string::npos);
}

TEST_CASE("the name entry screen echoes the typed name") {
    dos::Video v;
    ui::UiState state;
    state.screen = ui::UiState::Screen::NameEntry;
    state.inputName = "Ana";
    state.inputError = "Digite um nome antes de continuar.";
    Game g;
    ui::draw(v, g, state);

    CHECK(startsWith(rowText(v, 0), "BURACO - novo jogo"));
    bool echoed = false, error = false;
    for (int r = 0; r < ui::ROWS; ++r) {
        const std::string line = rowText(v, r);
        if (line.find("> Ana_") != std::string::npos) echoed = true;
        if (line.find("Digite um nome") != std::string::npos) error = true;
    }
    CHECK(echoed);
    CHECK(error);
}

TEST_CASE("the continue prompt shows the save file") {
    dos::Video v;
    ui::UiState state;
    state.screen = ui::UiState::Screen::ContinuePrompt;
    state.promptSave = "C:/jogos/Ana.sav";
    state.promptName = "Ana";
    Game g;
    ui::draw(v, g, state);

    CHECK(startsWith(rowText(v, 0), "BURACO - jogo gravado"));
    bool file = false, name = false;
    for (int r = 0; r < ui::ROWS; ++r) {
        const std::string line = rowText(v, r);
        if (line.find("C:/jogos/Ana.sav") != std::string::npos) file = true;
        if (line.find("Jogador: Ana") != std::string::npos) name = true;
    }
    CHECK(file);
    CHECK(name);
}

TEST_CASE("the scoring screen shows both columns") {
    Game g;
    const std::string text = testutil::saveHeader(int(Phase::HandOver)) +
                             testutil::playerLine(0, {}, true, 130, 130) +
                             testutil::playerLine(1, {53}, false, -20, 0);
    REQUIRE(testutil::loadText(g, text, "ui_score.sav"));

    dos::Video v;
    ui::UiState state;
    ui::draw(v, g, state);

    CHECK(rowText(v, 0).find("CONTAGEM DE PONTOS") != std::string::npos);
    CHECK(rowText(v, 0).find("mao 1") != std::string::npos);
    CHECK(rowText(v, 18).find("Jogos") != std::string::npos);
    CHECK(rowText(v, 22).find("Total da mao") != std::string::npos);
    CHECK(rowText(v, 24).find("ENTER para continuar") != std::string::npos);
}

TEST_CASE("the status screen reports the match numbers") {
    Game g;
    g.newMatch("Ana", 11u);
    dos::Video v;
    ui::UiState state;
    state.status = true;
    ui::draw(v, g, state);

    CHECK(startsWith(rowText(v, 0), "BURACO - situacao geral"));
    bool hands = false, melds = false;
    for (int r = 0; r < ui::ROWS; ++r) {
        const std::string line = rowText(v, r);
        if (line.find("cartas na mao") != std::string::npos) hands = true;
        if (line.find("jogos e eu") != std::string::npos) melds = true;
    }
    CHECK(hands);
    CHECK(melds);
}
