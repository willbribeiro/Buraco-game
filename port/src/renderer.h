// SDL2 presentation: text framebuffer -> texture -> window.
#pragma once

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

    bool init(const char* title, int scale);
    void present(Video& video, double timeSeconds);

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
};

}  // namespace dos
