#include "video.h"

#include <cstddef>

namespace dos {

Video::Video() {
    setEgaPalette();
    resetText();
}

void Video::setEgaPalette() {
    static const uint32_t ega[16] = {
        0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
        0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
        0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
        0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
    };
    for (int i = 0; i < 16; ++i) argb[i] = 0xFF000000u | ega[i];
}

void Video::resetText() {
    for (std::size_t i = 0; i < text.size(); i += 2) {
        text[i] = ' ';
        text[i + 1] = 0x07;
    }
}

}  // namespace dos
