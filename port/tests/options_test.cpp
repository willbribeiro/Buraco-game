#include "options.h"

#include "config.h"
#include "ui.h"
#include "video.h"

#include "test_framework.hpp"

#include <string>

using namespace options;

namespace {

std::string rowText(const dos::Video& v, int row) {
    std::string s;
    for (int c = 0; c < ui::COLS; ++c) s += char(v.text[(row * ui::COLS + c) * 2]);
    return s;
}

void select(State& state, Row row) { state.selected = int(row); }

}  // namespace

TEST_CASE("options navigation wraps through every row") {
    State state;
    config::DisplaySettings settings;

    CHECK_EQ(state.selected, 0);
    CHECK(handleKey(state, Key::Up, settings) == Action::None);
    CHECK_EQ(state.selected, kRowCount - 1);
    CHECK(handleKey(state, Key::Down, settings) == Action::None);
    CHECK_EQ(state.selected, 0);

    for (int i = 0; i < 4; ++i) handleKey(state, Key::Down, settings);
    CHECK_EQ(state.selected, 4);
    handleKey(state, Key::Up, settings);
    CHECK_EQ(state.selected, 3);
}

TEST_CASE("options scale row rings through auto and 1x..8x") {
    State state;
    config::DisplaySettings settings;
    select(state, Row::Scale);

    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.scale, 3);
    CHECK(handleKey(state, Key::Left, settings) == Action::Apply);
    CHECK_EQ(settings.scale, 2);

    settings.scale = 8;
    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.scale, 0);
    CHECK(handleKey(state, Key::Left, settings) == Action::Apply);
    CHECK_EQ(settings.scale, 8);

    settings.scale = 0;
    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.scale, 1);
    CHECK(handleKey(state, Key::Enter, settings) == Action::Apply);
    CHECK_EQ(settings.scale, 2);
}

TEST_CASE("options arrow keys set boolean rows on and off") {
    State state;
    config::DisplaySettings settings;

    select(state, Row::Adjustment);
    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.integerScale, true);
    CHECK(handleKey(state, Key::Left, settings) == Action::Apply);
    CHECK_EQ(settings.integerScale, false);
    CHECK(handleKey(state, Key::Enter, settings) == Action::Apply);
    CHECK_EQ(settings.integerScale, true);

    select(state, Row::Fullscreen);
    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.fullscreen, true);
    CHECK(handleKey(state, Key::Left, settings) == Action::Apply);
    CHECK_EQ(settings.fullscreen, false);

    select(state, Row::Vsync);
    CHECK(handleKey(state, Key::Left, settings) == Action::Apply);
    CHECK_EQ(settings.vsync, false);
    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.vsync, true);

    select(state, Row::Maximized);
    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.maximized, true);
}

TEST_CASE("options actions center, reset and close") {
    State state;
    config::DisplaySettings settings;
    settings.centered = false;

    select(state, Row::Center);
    CHECK(handleKey(state, Key::Enter, settings) == Action::Apply);
    CHECK_EQ(settings.centered, true);

    select(state, Row::Reset);
    settings.scale = 7;
    settings.vsync = false;
    settings.fullscreen = true;
    CHECK(handleKey(state, Key::Right, settings) == Action::None);
    CHECK(handleKey(state, Key::Enter, settings) == Action::Reset);
    CHECK(settings == config::defaults().display);

    select(state, Row::Close);
    CHECK(handleKey(state, Key::Enter, settings) == Action::Close);
    CHECK(handleKey(state, Key::Escape, settings) == Action::Close);
}

TEST_CASE("options text describes the live settings") {
    config::DisplaySettings settings;
    CHECK_EQ(label(Row::Scale), "Escala da janela");
    CHECK_EQ(value(Row::Scale, settings), "2x (1280x800)");

    settings.scale = 0;
    CHECK_EQ(value(Row::Scale, settings), "Auto (1280x800)");
    settings.scale = 3;
    CHECK_EQ(value(Row::Scale, settings), "3x (1920x1200)");

    settings.integerScale = true;
    CHECK_EQ(value(Row::Adjustment, settings), "Inteiro (pixel-perfect)");
    settings.integerScale = false;
    CHECK_EQ(value(Row::Adjustment, settings), "Esticado");

    settings.fullscreen = true;
    CHECK_EQ(value(Row::Fullscreen, settings), "Sim");
    settings.fullscreen = false;
    CHECK_EQ(value(Row::Fullscreen, settings), "Nao");

    settings.centered = false;
    CHECK(value(Row::Center, settings).rfind("Livre", 0) == 0);

    CHECK(isAction(Row::Center));
    CHECK(isAction(Row::Reset));
    CHECK(isAction(Row::Close));
    CHECK(!isAction(Row::Scale));
}

TEST_CASE("options screen shows the selection and the current values") {
    dos::Video v;
    config::DisplaySettings settings;
    settings.scale = 3;
    settings.vsync = false;

    ui::drawOptions(v, settings, 0);
    CHECK(rowText(v, 0).rfind("BURACO - opcoes", 0) == 0);
    CHECK(rowText(v, 3).find("Escala da janela") != std::string::npos);
    CHECK(rowText(v, 3).find("3x (1920x1200)") != std::string::npos);
    CHECK(rowText(v, 3)[2] == '>');
    CHECK(rowText(v, 6).find("VSync") != std::string::npos);
    CHECK(rowText(v, 6).find("Nao") != std::string::npos);
    CHECK(rowText(v, 14).find("Setas") != std::string::npos);

    ui::drawOptions(v, settings, 6);
    CHECK(rowText(v, 3)[2] == ' ');
    CHECK(rowText(v, 10)[2] == '>');
    CHECK(rowText(v, 10).find("Restaurar padroes") != std::string::npos);
}
