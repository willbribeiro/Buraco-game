#include "renderer.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace dos {

bool Renderer::init(const char* title, int scale) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;
    window_ = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, CANVAS_W * scale,
                               CANVAS_H * scale, SDL_WINDOW_RESIZABLE);
    renderer_ = SDL_CreateRenderer(
        window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, CANVAS_W, CANVAS_H);
    if (!window_ || !renderer_ || !texture_) return false;
    SDL_RenderSetLogicalSize(renderer_, CANVAS_W, CANVAS_H);
    canvas_.resize(CANVAS_W * CANVAS_H);
    loadFont();
    return true;
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
