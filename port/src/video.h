// Text-mode framebuffer used by the port.
//
// The game only ever runs in 80x25 CP437 text mode; the original BIOS calls
// were replaced by direct writes into this buffer.  Rendering to SDL lives in
// renderer.h so the buffer itself stays testable without SDL.
#pragma once

#include <array>
#include <cstdint>

namespace dos {

constexpr int TEXT_COLS = 80;
constexpr int TEXT_ROWS = 25;
constexpr int CANVAS_W = 640;  // 80 columns * 8 pixels, then scaled by 2
constexpr int CANVAS_H = 400;  // 25 rows * 16 pixels, then scaled by 2

struct Video {
    std::array<uint8_t, TEXT_COLS * TEXT_ROWS * 2> text{};  // char, attr
    std::array<uint32_t, 16> argb{};                        // 0xAARRGGBB
    bool blinkEnabled = true;

    Video();

    void resetText();

private:
    void setEgaPalette();
};

}  // namespace dos
