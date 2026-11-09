// SDL2 presentation: text framebuffer -> texture -> window.
#pragma once

#include "config.h"
#include "video.h"

#include <array>
#include <cstdint>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace dos {

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Creates the window.  `settings` is adjusted in place when the saved
    // position no longer falls on an existing monitor.
    bool init(const char* title, config::DisplaySettings& settings);

    // Applies display settings to an existing window: size, position,
    // fullscreen, maximized, VSync and integer scaling.  Safe to call with the
    // settings already in effect.
    void apply(const config::DisplaySettings& settings);

    void present(Video& video, double timeSeconds);

    bool isFullscreen() const;
    bool isMaximized() const;

    // Geometry this renderer last asked the window manager for.  The event
    // loop uses it to tell our own resize/move apart from a user action.
    int expectedWidth() const { return appliedWidth_; }
    int expectedHeight() const { return appliedHeight_; }
    int expectedX() const { return appliedX_; }
    int expectedY() const { return appliedY_; }

private:
    void loadFont();
    void drawText(const Video& video, double timeSeconds);
    static void makeFallbackGlyph(uint8_t ch, std::array<uint8_t, 16>& out);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    std::vector<uint32_t> canvas_;
    std::array<uint8_t, 256 * 16> font_{};
    bool fontLoaded_ = false;
    int appliedWidth_ = 0;
    int appliedHeight_ = 0;
    int appliedX_ = 0;
    int appliedY_ = 0;
};

}  // namespace dos
