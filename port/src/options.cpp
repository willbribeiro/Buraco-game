#include "options.h"

#include "window_scale.h"

#include <algorithm>

namespace options {
namespace {

Row rowAt(int index) {
    const int clamped = std::clamp(index, 0, kRowCount - 1);
    return static_cast<Row>(clamped);
}

// 0 (automatic) and 1..8 form a single ring so both directions work.
int nextScale(int scale, int delta) {
    int value = std::clamp(scale, 0, 8) + delta;
    if (value > 8) value = 0;
    if (value < 0) value = 8;
    return value;
}

bool nextFlag(bool value, Key key) {
    if (key == Key::Left) return false;
    if (key == Key::Right) return true;
    return !value;
}

std::string scaleText(const config::DisplaySettings& settings) {
    if (settings.scale <= 0)
        return "Auto (" + std::to_string(settings.windowWidth) + "x" +
               std::to_string(settings.windowHeight) + ")";
    const window_scale::Size size = window_scale::sizeForScale(settings.scale);
    return std::to_string(settings.scale) + "x (" + std::to_string(size.width) + "x" +
           std::to_string(size.height) + ")";
}

}  // namespace

bool isAction(Row row) {
    return row == Row::Center || row == Row::Reset || row == Row::Close;
}

std::string label(Row row) {
    switch (row) {
        case Row::Scale: return "Escala da janela";
        case Row::Adjustment: return "Ajuste da imagem";
        case Row::Fullscreen: return "Tela cheia";
        case Row::Vsync: return "VSync";
        case Row::Maximized: return "Iniciar maximizada";
        case Row::Sound: return "Som";
        case Row::Volume: return "Volume";
        case Row::Center: return "Centralizar janela";
        case Row::Reset: return "Restaurar padroes";
        case Row::Close: return "Voltar ao jogo";
        case Row::Count: break;
    }
    return {};
}

std::string value(Row row, const config::Settings& settings) {
    switch (row) {
        case Row::Scale:
            return scaleText(settings.display);
        case Row::Adjustment:
            return settings.display.integerScale ? "Inteiro (pixel-perfect)" : "Esticado";
        case Row::Fullscreen:
            return settings.display.fullscreen ? "Sim" : "Nao";
        case Row::Vsync:
            return settings.display.vsync ? "Sim" : "Nao";
        case Row::Maximized:
            return settings.display.maximized ? "Sim" : "Nao";
        case Row::Sound:
            return settings.sound.enabled ? "Sim" : "Nao";
        case Row::Volume:
            return std::to_string(settings.sound.volume) + "%";
        case Row::Center:
            return settings.display.centered ? "Centralizada" : "Livre (ENTER centraliza)";
        case Row::Reset:
            return "ENTER";
        case Row::Close:
            return "ENTER ou ESC";
        case Row::Count:
            break;
    }
    return {};
}

Action handleKey(State& state, Key key, config::Settings& settings) {
    if (key == Key::Up) {
        state.selected = (state.selected + kRowCount - 1) % kRowCount;
        return Action::None;
    }
    if (key == Key::Down) {
        state.selected = (state.selected + 1) % kRowCount;
        return Action::None;
    }
    if (key == Key::Escape) return Action::Close;

    const Row row = rowAt(state.selected);
    const bool activate = key == Key::Enter || key == Key::Left || key == Key::Right;
    if (!activate) return Action::None;

    switch (row) {
        case Row::Scale:
            settings.display.scale =
                nextScale(settings.display.scale, key == Key::Left ? -1 : 1);
            return Action::Apply;
        case Row::Adjustment:
            settings.display.integerScale = nextFlag(settings.display.integerScale, key);
            return Action::Apply;
        case Row::Fullscreen:
            settings.display.fullscreen = nextFlag(settings.display.fullscreen, key);
            return Action::Apply;
        case Row::Vsync:
            settings.display.vsync = nextFlag(settings.display.vsync, key);
            return Action::Apply;
        case Row::Maximized:
            settings.display.maximized = nextFlag(settings.display.maximized, key);
            return Action::Apply;
        case Row::Sound:
            settings.sound.enabled = nextFlag(settings.sound.enabled, key);
            return Action::Apply;
        case Row::Volume: {
            const int step = key == Key::Left ? -10 : 10;
            settings.sound.volume = std::clamp(settings.sound.volume + step, 0, 100);
            return Action::Apply;
        }
        case Row::Center:
            if (key == Key::Enter || key == Key::Left || key == Key::Right) {
                settings.display.centered = true;
                return Action::Apply;
            }
            return Action::None;
        case Row::Reset:
            if (key == Key::Enter) {
                settings = config::defaults();
                return Action::Reset;
            }
            return Action::None;
        case Row::Close:
            if (key == Key::Enter) return Action::Close;
            return Action::None;
        case Row::Count:
            break;
    }
    return Action::None;
}

}  // namespace options
