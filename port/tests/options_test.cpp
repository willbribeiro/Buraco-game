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
    config::Settings settings;

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
    config::Settings settings;
    select(state, Row::Scale);

    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.display.scale, 3);
    CHECK(handleKey(state, Key::Left, settings) == Action::Apply);
    CHECK_EQ(settings.display.scale, 2);

    settings.display.scale = 8;
    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.display.scale, 0);
    CHECK(handleKey(state, Key::Left, settings) == Action::Apply);
    CHECK_EQ(settings.display.scale, 8);

    settings.display.scale = 0;
    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.display.scale, 1);
    CHECK(handleKey(state, Key::Enter, settings) == Action::Apply);
    CHECK_EQ(settings.display.scale, 2);
}

TEST_CASE("options arrow keys set boolean display rows on and off") {
    State state;
    config::Settings settings;

    select(state, Row::Adjustment);
    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.display.integerScale, true);
    CHECK(handleKey(state, Key::Left, settings) == Action::Apply);
    CHECK_EQ(settings.display.integerScale, false);
    CHECK(handleKey(state, Key::Enter, settings) == Action::Apply);
    CHECK_EQ(settings.display.integerScale, true);

    select(state, Row::Fullscreen);
    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.display.fullscreen, true);
    CHECK(handleKey(state, Key::Left, settings) == Action::Apply);
    CHECK_EQ(settings.display.fullscreen, false);

    select(state, Row::Vsync);
    CHECK(handleKey(state, Key::Left, settings) == Action::Apply);
    CHECK_EQ(settings.display.vsync, false);
    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.display.vsync, true);

    select(state, Row::Maximized);
    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.display.maximized, true);
}

TEST_CASE("options sound rows toggle audio and step the volume") {
    State state;
    config::Settings settings;
    CHECK_EQ(settings.sound.enabled, true);
    CHECK_EQ(settings.sound.volume, 70);

    select(state, Row::Sound);
    CHECK(handleKey(state, Key::Left, settings) == Action::Apply);
    CHECK_EQ(settings.sound.enabled, false);
    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.sound.enabled, true);
    CHECK(handleKey(state, Key::Enter, settings) == Action::Apply);
    CHECK_EQ(settings.sound.enabled, false);

    select(state, Row::Volume);
    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.sound.volume, 80);
    CHECK(handleKey(state, Key::Left, settings) == Action::Apply);
    CHECK_EQ(settings.sound.volume, 70);

    settings.sound.volume = 95;
    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.sound.volume, 100);
    CHECK(handleKey(state, Key::Right, settings) == Action::Apply);
    CHECK_EQ(settings.sound.volume, 100);

    settings.sound.volume = 5;
    CHECK(handleKey(state, Key::Left, settings) == Action::Apply);
    CHECK_EQ(settings.sound.volume, 0);
    CHECK(handleKey(state, Key::Left, settings) == Action::Apply);
    CHECK_EQ(settings.sound.volume, 0);
}

TEST_CASE("options actions center, reset and close") {
    State state;
    config::Settings settings;
    settings.display.centered = false;

    select(state, Row::Center);
    CHECK(handleKey(state, Key::Enter, settings) == Action::Apply);
    CHECK_EQ(settings.display.centered, true);

    select(state, Row::Reset);
    settings.display.scale = 7;
    settings.display.vsync = false;
    settings.display.fullscreen = true;
    settings.sound.enabled = false;
    settings.sound.volume = 10;
    CHECK(handleKey(state, Key::Right, settings) == Action::None);
    CHECK(handleKey(state, Key::Enter, settings) == Action::Reset);
    CHECK(settings == config::defaults());

    select(state, Row::Close);
    CHECK(handleKey(state, Key::Enter, settings) == Action::Close);
    CHECK(handleKey(state, Key::Escape, settings) == Action::Close);
}

TEST_CASE("options text describes the live settings") {
    config::Settings settings;
    CHECK_EQ(label(Row::Scale), "Escala da janela");
    CHECK_EQ(value(Row::Scale, settings), "2x (1280x800)");

    settings.display.scale = 0;
    CHECK_EQ(value(Row::Scale, settings), "Auto (1280x800)");
    settings.display.scale = 3;
    CHECK_EQ(value(Row::Scale, settings), "3x (1920x1200)");

    settings.display.integerScale = true;
    CHECK_EQ(value(Row::Adjustment, settings), "Inteiro (pixel-perfect)");
    settings.display.integerScale = false;
    CHECK_EQ(value(Row::Adjustment, settings), "Esticado");

    settings.display.fullscreen = true;
    CHECK_EQ(value(Row::Fullscreen, settings), "Sim");
    settings.display.fullscreen = false;
    CHECK_EQ(value(Row::Fullscreen, settings), "Nao");

    settings.display.centered = false;
    CHECK(value(Row::Center, settings).rfind("Livre", 0) == 0);

    CHECK_EQ(label(Row::Sound), "Som");
    CHECK_EQ(value(Row::Sound, settings), "Sim");
    settings.sound.enabled = false;
    CHECK_EQ(value(Row::Sound, settings), "Nao");
    CHECK_EQ(label(Row::Volume), "Volume");
    CHECK_EQ(value(Row::Volume, settings), "70%");

    CHECK(isAction(Row::Center));
    CHECK(isAction(Row::Reset));
    CHECK(isAction(Row::Close));
    CHECK(!isAction(Row::Scale));
    CHECK(!isAction(Row::Sound));
}

TEST_CASE("options screen shows the selection and the current values") {
    dos::Video v;
    config::Settings settings;
    settings.display.scale = 3;
    settings.display.vsync = false;
    settings.sound.volume = 40;

    ui::drawOptions(v, settings, 0);
    CHECK(rowText(v, 0).rfind("BURACO - opcoes", 0) == 0);
    CHECK(rowText(v, 3).find("Escala da janela") != std::string::npos);
    CHECK(rowText(v, 3).find("3x (1920x1200)") != std::string::npos);
    CHECK(rowText(v, 3)[2] == '>');
    CHECK(rowText(v, 6).find("VSync") != std::string::npos);
    CHECK(rowText(v, 6).find("Nao") != std::string::npos);
    CHECK(rowText(v, 9).find("Som") != std::string::npos);
    CHECK(rowText(v, 10).find("Volume") != std::string::npos);
    CHECK(rowText(v, 10).find("40%") != std::string::npos);
    CHECK(rowText(v, 16).find("Setas") != std::string::npos);

    ui::drawOptions(v, settings, int(Row::Reset));
    CHECK(rowText(v, 3)[2] == ' ');
    CHECK(rowText(v, 13)[2] == '>');
    CHECK(rowText(v, 13).find("Restaurar padroes") != std::string::npos);
    CHECK(rowText(v, 14).find("Voltar ao jogo") != std::string::npos);
}
