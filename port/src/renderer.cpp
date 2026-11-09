#include "renderer.h"

#include <SDL.h>

#include "window_scale.h"
#include "window_state.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace dos {
namespace {

struct DisplayInfo {
    SDL_Rect bounds{0, 0, 0, 0};
    SDL_Rect usable{0, 0, 0, 0};
};

std::vector<DisplayInfo> collectDisplays() {
    std::vector<DisplayInfo> displays;
    const int count = SDL_GetNumVideoDisplays();
    for (int i = 0; i < count; ++i) {
        DisplayInfo info;
        if (SDL_GetDisplayBounds(i, &info.bounds) != 0)
            info.bounds = SDL_Rect{0, 0, 0, 0};
        if (SDL_GetDisplayUsableBounds(i, &info.usable) != 0)
            info.usable = info.bounds;
        displays.push_back(info);
    }
    return displays;
}

std::vector<window_scale::Rect> displayRects(const std::vector<DisplayInfo>& displays) {
    std::vector<window_scale::Rect> rects;
    rects.reserve(displays.size());
    for (const DisplayInfo& display : displays)
        rects.push_back(window_scale::Rect{display.bounds.x, display.bounds.y,
                                           display.bounds.w, display.bounds.h});
    return rects;
}

const DisplayInfo* displayContaining(const std::vector<DisplayInfo>& displays, int x, int y) {
    for (const DisplayInfo& display : displays) {
        if (x >= display.bounds.x && x < display.bounds.x + display.bounds.w &&
            y >= display.bounds.y && y < display.bounds.y + display.bounds.h)
            return &display;
    }
    return displays.empty() ? nullptr : &displays.front();
}

window_scale::Size windowSizeFor(const config::DisplaySettings& settings,
                                 const DisplayInfo* display) {
    return window_scale::windowSize(
        settings.windowWidth, settings.windowHeight, settings.scale,
        display ? display->usable.w : 0, display ? display->usable.h : 0);
}

void rememberPosition(SDL_Window* window, int& x, int& y) {
    x = 0;
    y = 0;
    if (window) SDL_GetWindowPosition(window, &x, &y);
}

}  // namespace

bool Renderer::init(const char* title, config::DisplaySettings& settings) {
    // These hints must be set before SDL_Init/CreateWindow.  Nearest sampling
    // keeps the VGA glyphs crisp; per-monitor DPI awareness avoids the blurry
    // bitmap scaling Windows applies to DPI-unaware windows.
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;

    const std::vector<DisplayInfo> displays = collectDisplays();
    // A config saved on a monitor that is no longer connected must not open
    // the window where the user cannot reach it.
    window_state::ensureVisible(settings, displayRects(displays));

    const DisplayInfo* display =
        settings.centered ? (displays.empty() ? nullptr : &displays.front())
                          : displayContaining(displays, settings.windowX, settings.windowY);
    const window_scale::Size size = windowSizeFor(settings, display);

    const int x = settings.centered ? SDL_WINDOWPOS_CENTERED : settings.windowX;
    const int y = settings.centered ? SDL_WINDOWPOS_CENTERED : settings.windowY;
    window_ = SDL_CreateWindow(title, x, y, size.width, size.height,
                               SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window_) {
        SDL_Quit();
        return false;
    }
    appliedWidth_ = size.width;
    appliedHeight_ = size.height;
    rememberPosition(window_, appliedX_, appliedY_);

    Uint32 rendererFlags = SDL_RENDERER_ACCELERATED;
    if (settings.vsync) rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
    renderer_ = SDL_CreateRenderer(window_, -1, rendererFlags);
    if (!renderer_)
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);

    auto fail = [this]() {
        if (texture_) {
            SDL_DestroyTexture(texture_);
            texture_ = nullptr;
        }
        if (renderer_) {
            SDL_DestroyRenderer(renderer_);
            renderer_ = nullptr;
        }
        if (window_) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }
        SDL_Quit();
        return false;
    };

    if (!renderer_) return fail();

    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, CANVAS_W, CANVAS_H);
    if (!texture_) return fail();

    SDL_RenderSetLogicalSize(renderer_, CANVAS_W, CANVAS_H);
    SDL_RenderSetIntegerScale(renderer_, settings.integerScale ? SDL_TRUE : SDL_FALSE);
    SDL_SetWindowMinimumSize(window_, window_scale::kMinWidth, window_scale::kMinHeight);
    if (settings.fullscreen)
        SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
    else if (settings.maximized)
        SDL_MaximizeWindow(window_);

    canvas_.resize(CANVAS_W * CANVAS_H);
    loadFont();
    return true;
}

void Renderer::apply(const config::DisplaySettings& settings) {
    if (!window_ || !renderer_) return;

    // SDL_RenderSetVSync exists since 2.0.18; failure only means the driver
    // cannot change it at runtime, so the frame rate simply stays as it was.
    if (SDL_RenderSetVSync(renderer_, settings.vsync ? 1 : 0) != 0)
        std::fprintf(stderr, "vsync: %s\n", SDL_GetError());
    SDL_RenderSetIntegerScale(renderer_, settings.integerScale ? SDL_TRUE : SDL_FALSE);

    if (settings.fullscreen != isFullscreen())
        SDL_SetWindowFullscreen(window_,
                               settings.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    if (settings.fullscreen) return;

    if (isMaximized()) {
        if (!settings.maximized) SDL_RestoreWindow(window_);
        else return;  // the window manager owns the geometry while maximized
    }
    if (settings.maximized) {
        SDL_MaximizeWindow(window_);
        return;
    }

    const std::vector<DisplayInfo> displays = collectDisplays();
    const DisplayInfo* display =
        settings.centered ? (displays.empty() ? nullptr : &displays.front())
                          : displayContaining(displays, settings.windowX, settings.windowY);
    const window_scale::Size size = windowSizeFor(settings, display);

    SDL_SetWindowSize(window_, size.width, size.height);
    if (settings.centered)
        SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    else
        SDL_SetWindowPosition(window_, settings.windowX, settings.windowY);

    appliedWidth_ = size.width;
    appliedHeight_ = size.height;
    rememberPosition(window_, appliedX_, appliedY_);
}

bool Renderer::isFullscreen() const {
    const Uint32 flags = window_ ? SDL_GetWindowFlags(window_) : 0;
    return (flags & SDL_WINDOW_FULLSCREEN) != 0;
}

bool Renderer::isMaximized() const {
    const Uint32 flags = window_ ? SDL_GetWindowFlags(window_) : 0;
    return (flags & SDL_WINDOW_MAXIMIZED) != 0;
}

Renderer::~Renderer() {
    if (texture_) SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

void Renderer::present(Video& video, double timeSeconds) {
    drawText(video, timeSeconds);
    SDL_UpdateTexture(texture_, nullptr, canvas_.data(),
                      CANVAS_W * int(sizeof(uint32_t)));
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
}

void Renderer::loadFont() {
    char* base = SDL_GetBasePath();
    const std::string path = std::string(base ? base : "") + "assets/font8x16.bin";
    if (base) SDL_free(base);
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        std::fprintf(stderr, "[dos] font not found: %s (using fallback glyphs)\n",
                     path.c_str());
        return;
    }
    const size_t n = std::fread(font_.data(), 1, font_.size(), f);
    std::fclose(f);
    fontLoaded_ = (n == font_.size());
}

void Renderer::drawText(const Video& video, double t) {
    const bool blink = video.blinkEnabled && std::fmod(t, 1.0) < 0.5;
    std::fill(canvas_.begin(), canvas_.end(), 0xFF000000u);
    for (int y = 0; y < TEXT_ROWS; ++y) {
        for (int x = 0; x < TEXT_COLS; ++x) {
            const int o = (y * TEXT_COLS + x) * 2;
            const uint8_t ch = video.text[o];
            const uint8_t attr = video.text[o + 1];
            const uint32_t fg = video.argb[attr & 0x0F];
            const uint32_t bg = video.argb[(attr >> 4) & 0x0F];
            const bool hidden = (attr & 0x80) && blink;

            const uint8_t* glyph = nullptr;
            std::array<uint8_t, 16> fallback{};
            if (fontLoaded_) {
                glyph = &font_[size_t(ch) * 16];
            } else {
                makeFallbackGlyph(ch, fallback);
                glyph = fallback.data();
            }

            for (int row = 0; row < 16; ++row) {
                const uint8_t bits = hidden ? 0 : glyph[row];
                uint32_t* dst = &canvas_[(y * 16 + row) * CANVAS_W + x * 8];
                for (int col = 0; col < 8; ++col)
                    dst[col] = (bits & (0x80 >> col)) ? fg : bg;
            }
        }
    }
}

void Renderer::makeFallbackGlyph(uint8_t ch, std::array<uint8_t, 16>& out) {
    out.fill(0);
    out[2] = 0xFF;
    out[13] = 0xFF;
    for (int row = 3; row < 13; ++row) {
        uint8_t bits = uint8_t(0x81 | (ch & 0x7E));
        if ((ch >> (row % 6)) & 1) bits ^= 0x7E;
        out[row] = bits;
    }
}

}  // namespace dos
