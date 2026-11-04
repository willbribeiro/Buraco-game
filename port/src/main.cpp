// Buraco (MS-DOS) -> SDL2 platform skeleton.
//
// This file is the *platform layer* only. It emulates the BIOS/DOS services
// the original game uses (INT 10h video, INT 16h keyboard, INT 33h mouse,
// PIT ticks, VGA DAC ports) backed by a virtual framebuffer. The recovered
// 16-bit logic can call these functions with the same register convention as
// the original code, which makes the 1:1 translation from Ghidra practical.
//
// Build: see CMakeLists.txt (vcpkg install sdl2).

#include <SDL.h>

#include "cards.h"
#include "game.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <istream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

namespace dos {

constexpr int TEXT_COLS = 80;
constexpr int TEXT_ROWS = 25;
constexpr int VGA_W = 320;
constexpr int VGA_H = 200;
constexpr int CANVAS_W = 640;
constexpr int CANVAS_H = 400;

// ---------------------------------------------------------------------------
// 16-bit register file (only the fields the BIOS shims need).
// ---------------------------------------------------------------------------
struct Regs16 {
    uint16_t ax = 0, bx = 0, cx = 0, dx = 0;
    uint16_t si = 0, di = 0, bp = 0, sp = 0;
    uint16_t ds = 0, es = 0, ss = 0;
    uint16_t flags = 0x0202;  // IF=1

    uint8_t ah() const { return uint8_t(ax >> 8); }
    uint8_t al() const { return uint8_t(ax); }
    uint8_t bh() const { return uint8_t(bx >> 8); }
    uint8_t bl() const { return uint8_t(bx); }
    uint8_t ch() const { return uint8_t(cx >> 8); }
    uint8_t cl() const { return uint8_t(cx); }
    uint8_t dh() const { return uint8_t(dx >> 8); }
    uint8_t dl() const { return uint8_t(dx); }

    void setAh(uint8_t v) { ax = uint16_t((ax & 0x00FF) | (v << 8)); }
    void setAl(uint8_t v) { ax = uint16_t((ax & 0xFF00) | v); }
};

// ---------------------------------------------------------------------------
// Virtual video: 80x25 text VRAM + mode 13h linear framebuffer + DAC.
// ---------------------------------------------------------------------------
struct Video {
    uint8_t mode = 0x03;
    std::array<uint8_t, TEXT_COLS * TEXT_ROWS * 2> text{};
    std::array<uint8_t, VGA_W * VGA_H> vga{};
    std::array<uint32_t, 256> argb{};              // 0xAARRGGBB, ready for SDL
    std::array<std::array<uint8_t, 3>, 256> dac{};  // 6-bit VGA DAC

    int dacIndex = 0;
    int dacComponent = 0;
    int cursorX = 0, cursorY = 0;
    bool cursorVisible = true;
    bool blinkEnabled = true;

    Video() {
        setEgaPalette();
        resetText();
    }

    static uint8_t expand6(uint8_t v) {
        return uint8_t((v << 2) | (v >> 4));
    }

    void setDac(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
        dac[index] = {uint8_t(r & 0x3F), uint8_t(g & 0x3F), uint8_t(b & 0x3F)};
        const uint8_t r8 = expand6(dac[index][0]);
        const uint8_t g8 = expand6(dac[index][1]);
        const uint8_t b8 = expand6(dac[index][2]);
        argb[index] = 0xFF000000u | (uint32_t(r8) << 16) |
                      (uint32_t(g8) << 8) | uint32_t(b8);
    }

    // VGA DAC is written as three consecutive bytes (6 bits each) on port 3C9.
    void dacWrite(uint8_t value) {
        static thread_local uint8_t pending[3] = {0, 0, 0};
        pending[dacComponent++] = uint8_t(value & 0x3F);
        if (dacComponent == 3) {
            setDac(uint8_t(dacIndex), pending[0], pending[1], pending[2]);
            dacComponent = 0;
            dacIndex = uint8_t(dacIndex + 1);
        }
    }

    void setEgaPalette() {
        static const uint32_t ega[16] = {
            0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
            0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
            0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
            0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
        };
        for (int i = 0; i < 16; ++i) {
            argb[i] = 0xFF000000u | ega[i];
            dac[i] = {uint8_t(((ega[i] >> 16) & 0xFF) >> 2),
                      uint8_t(((ega[i] >> 8) & 0xFF) >> 2),
                      uint8_t((ega[i] & 0xFF) >> 2)};
        }
    }

    void setDefaultVgaPalette() {
        setEgaPalette();
        for (int i = 16; i < 32; ++i) {
            const uint8_t g = uint8_t((i - 16) * 4);
            setDac(uint8_t(i), g, g, g);
        }
        for (int i = 32; i < 248; ++i) {
            const int n = i - 32;
            const uint8_t r = uint8_t((n / 36) % 6) * 12;
            const uint8_t g = uint8_t((n / 6) % 6) * 12;
            const uint8_t b = uint8_t(n % 6) * 12;
            setDac(uint8_t(i), r, g, b);
        }
        for (int i = 248; i < 256; ++i) setDac(uint8_t(i), 0, 0, 0);
    }

    void resetText() {
        for (size_t i = 0; i < text.size(); i += 2) {
            text[i] = ' ';
            text[i + 1] = 0x07;
        }
        cursorX = cursorY = 0;
    }

    void setMode(uint8_t m) {
        mode = m;
        if (m == 0x13) {
            vga.fill(0);
            setDefaultVgaPalette();
        } else {
            resetText();
            setEgaPalette();
        }
    }

    int cellOffset(int x, int y) const { return (y * TEXT_COLS + x) * 2; }

    void scrollUp(int x0, int y0, int x1, int y1, int lines, uint8_t attr) {
        clampWindow(x0, y0, x1, y1);
        const int height = y1 - y0 + 1;
        const int width = x1 - x0 + 1;
        if (lines <= 0 || lines >= height) {
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x) {
                    const int o = cellOffset(x, y);
                    text[o] = ' ';
                    text[o + 1] = attr;
                }
            return;
        }
        for (int y = y0; y <= y1 - lines; ++y)
            for (int x = x0; x <= x1; ++x)
                std::memcpy(&text[cellOffset(x, y)],
                            &text[cellOffset(x, y + lines)], 2);
        for (int y = y1 - lines + 1; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const int o = cellOffset(x, y);
                text[o] = ' ';
                text[o + 1] = attr;
            }
        (void)width;
    }

    void scrollDown(int x0, int y0, int x1, int y1, int lines, uint8_t attr) {
        clampWindow(x0, y0, x1, y1);
        const int height = y1 - y0 + 1;
        if (lines <= 0 || lines >= height) {
            scrollUp(x0, y0, x1, y1, 0, attr);
            return;
        }
        for (int y = y1; y >= y0 + lines; --y)
            for (int x = x0; x <= x1; ++x)
                std::memcpy(&text[cellOffset(x, y)],
                            &text[cellOffset(x, y - lines)], 2);
        for (int y = y0; y < y0 + lines; ++y)
            for (int x = x0; x <= x1; ++x) {
                const int o = cellOffset(x, y);
                text[o] = ' ';
                text[o + 1] = attr;
            }
    }

    static void clampWindow(int& x0, int& y0, int& x1, int& y1) {
        x0 = std::clamp(x0, 0, TEXT_COLS - 1);
        x1 = std::clamp(x1, 0, TEXT_COLS - 1);
        y0 = std::clamp(y0, 0, TEXT_ROWS - 1);
        y1 = std::clamp(y1, 0, TEXT_ROWS - 1);
        if (x0 > x1) std::swap(x0, x1);
        if (y0 > y1) std::swap(y0, y1);
    }

    void putChar(uint8_t ch, uint8_t attr, int count) {
        for (int i = 0; i < count; ++i) {
            if (cursorX >= TEXT_COLS) {
                cursorX = 0;
                ++cursorY;
            }
            if (cursorY >= TEXT_ROWS) {
                scrollUp(0, 0, TEXT_COLS - 1, TEXT_ROWS - 1, 1, attr);
                cursorY = TEXT_ROWS - 1;
            }
            const int o = cellOffset(cursorX, cursorY);
            text[o] = ch;
            text[o + 1] = attr;
            ++cursorX;
        }
    }

    void teletype(uint8_t ch) {
        switch (ch) {
            case 0x07:  // BEL
                break;
            case 0x08:  // BS
                if (cursorX > 0) --cursorX;
                break;
            case 0x09:  // TAB
                cursorX = std::min(TEXT_COLS - 1, (cursorX + 8) & ~7);
                break;
            case 0x0A:  // LF
                ++cursorY;
                if (cursorY >= TEXT_ROWS) {
                    scrollUp(0, 0, TEXT_COLS - 1, TEXT_ROWS - 1, 1, 0x07);
                    cursorY = TEXT_ROWS - 1;
                }
                break;
            case 0x0D:  // CR
                cursorX = 0;
                break;
            default:
                putChar(ch, 0x07, 1);
                break;
        }
    }
};

// ---------------------------------------------------------------------------
// INT 10h - BIOS video services used by text-mode and mode-13h DOS games.
// ---------------------------------------------------------------------------
inline void int10(Video& v, Regs16& r) {
    switch (r.ah()) {
        case 0x00:  // set video mode
            v.setMode(r.al());
            break;
        case 0x01:  // set cursor shape (ignored: native cursor is drawn by us)
            v.cursorVisible = (r.cx & 0x1F) < 0x20;
            break;
        case 0x02:  // set cursor position
            v.cursorX = std::clamp<int>(r.dl(), 0, TEXT_COLS - 1);
            v.cursorY = std::clamp<int>(r.dh(), 0, TEXT_ROWS - 1);
            break;
        case 0x03:  // get cursor position
            r.cx = uint16_t(0x0607);
            r.dx = uint16_t((v.cursorY << 8) | v.cursorX);
            break;
        case 0x05:  // set active page (single page port)
            break;
        case 0x06:  // scroll window up
            v.scrollUp(r.cl(), r.ch(), r.dl(), r.dh(), r.al(), r.bh());
            break;
        case 0x07:  // scroll window down
            v.scrollDown(r.cl(), r.ch(), r.dl(), r.dh(), r.al(), r.bh());
            break;
        case 0x08: {  // read char/attr
            const int o = v.cellOffset(v.cursorX, v.cursorY);
            r.ax = uint16_t(v.text[o] | (v.text[o + 1] << 8));
            break;
        }
        case 0x09:    // write char + attr, count = CX
        case 0x0A: {  // write char, keep attr
            const uint8_t attr = (r.ah() == 0x09) ? r.bl()
                                                  : v.text[v.cellOffset(v.cursorX, v.cursorY) + 1];
            v.putChar(r.al(), attr, std::max<int>(1, r.cx));
            break;
        }
        case 0x0C:  // write pixel (mode 13h)
            if (v.mode == 0x13) {
                const int x = r.cx, y = r.dx;
                if (x >= 0 && x < VGA_W && y >= 0 && y < VGA_H)
                    v.vga[y * VGA_W + x] = r.al();
            }
            break;
        case 0x0D:  // read pixel
            if (v.mode == 0x13 && r.cx < VGA_W && r.dx < VGA_H)
                r.setAl(v.vga[r.dx * VGA_W + r.cx]);
            break;
        case 0x0E:  // teletype
            v.teletype(r.al());
            break;
        case 0x0F:  // get mode
            r.setAl(v.mode);
            r.setAh(uint8_t(TEXT_COLS));
            r.bx &= 0x00FF;  // BH = active page
            break;
        case 0x10:  // palette / DAC
            if (r.al() == 0x00) {
                v.setDac(r.bl(), r.dh(), r.ch(), r.cl());
            } else if (r.al() == 0x02) {
                // ES:DX points to 256 RGB triples. In the full port, read
                // from the flat 1 MB memory model; kept as a hook here.
            }
            break;
        case 0x12:  // alternate select / EGA config
            break;
        case 0x13:  // write string (ES:BP, CX chars, DH row, DL col)
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// INT 16h - keyboard. BIOS set-1 scan codes; AX = scan<<8 | ascii.
// ---------------------------------------------------------------------------
inline uint8_t biosScan(SDL_Scancode sc) {
    switch (sc) {
        case SDL_SCANCODE_ESCAPE: return 0x01;
        case SDL_SCANCODE_1: return 0x02;
        case SDL_SCANCODE_2: return 0x03;
        case SDL_SCANCODE_3: return 0x04;
        case SDL_SCANCODE_4: return 0x05;
        case SDL_SCANCODE_5: return 0x06;
        case SDL_SCANCODE_6: return 0x07;
        case SDL_SCANCODE_7: return 0x08;
        case SDL_SCANCODE_8: return 0x09;
        case SDL_SCANCODE_9: return 0x0A;
        case SDL_SCANCODE_0: return 0x0B;
        case SDL_SCANCODE_MINUS: return 0x0C;
        case SDL_SCANCODE_EQUALS: return 0x0D;
        case SDL_SCANCODE_BACKSPACE: return 0x0E;
        case SDL_SCANCODE_TAB: return 0x0F;
        case SDL_SCANCODE_Q: return 0x10;
        case SDL_SCANCODE_W: return 0x11;
        case SDL_SCANCODE_E: return 0x12;
        case SDL_SCANCODE_R: return 0x13;
        case SDL_SCANCODE_T: return 0x14;
        case SDL_SCANCODE_Y: return 0x15;
        case SDL_SCANCODE_U: return 0x16;
        case SDL_SCANCODE_I: return 0x17;
        case SDL_SCANCODE_O: return 0x18;
        case SDL_SCANCODE_P: return 0x19;
        case SDL_SCANCODE_LEFTBRACKET: return 0x1A;
        case SDL_SCANCODE_RIGHTBRACKET: return 0x1B;
        case SDL_SCANCODE_RETURN: return 0x1C;
        case SDL_SCANCODE_LCTRL: return 0x1D;
        case SDL_SCANCODE_A: return 0x1E;
        case SDL_SCANCODE_S: return 0x1F;
        case SDL_SCANCODE_D: return 0x20;
        case SDL_SCANCODE_F: return 0x21;
        case SDL_SCANCODE_G: return 0x22;
        case SDL_SCANCODE_H: return 0x23;
        case SDL_SCANCODE_J: return 0x24;
        case SDL_SCANCODE_K: return 0x25;
        case SDL_SCANCODE_L: return 0x26;
        case SDL_SCANCODE_SEMICOLON: return 0x27;
        case SDL_SCANCODE_APOSTROPHE: return 0x28;
        case SDL_SCANCODE_GRAVE: return 0x29;
        case SDL_SCANCODE_LSHIFT: return 0x2A;
        case SDL_SCANCODE_BACKSLASH: return 0x2B;
        case SDL_SCANCODE_Z: return 0x2C;
        case SDL_SCANCODE_X: return 0x2D;
        case SDL_SCANCODE_C: return 0x2E;
        case SDL_SCANCODE_V: return 0x2F;
        case SDL_SCANCODE_B: return 0x30;
        case SDL_SCANCODE_N: return 0x31;
        case SDL_SCANCODE_M: return 0x32;
        case SDL_SCANCODE_COMMA: return 0x33;
        case SDL_SCANCODE_PERIOD: return 0x34;
        case SDL_SCANCODE_SLASH: return 0x35;
        case SDL_SCANCODE_RSHIFT: return 0x36;
        case SDL_SCANCODE_KP_MULTIPLY: return 0x37;
        case SDL_SCANCODE_LALT: return 0x38;
        case SDL_SCANCODE_SPACE: return 0x39;
        case SDL_SCANCODE_CAPSLOCK: return 0x3A;
        case SDL_SCANCODE_F1: return 0x3B;
        case SDL_SCANCODE_F2: return 0x3C;
        case SDL_SCANCODE_F3: return 0x3D;
        case SDL_SCANCODE_F4: return 0x3E;
        case SDL_SCANCODE_F5: return 0x3F;
        case SDL_SCANCODE_F6: return 0x40;
        case SDL_SCANCODE_F7: return 0x41;
        case SDL_SCANCODE_F8: return 0x42;
        case SDL_SCANCODE_F9: return 0x43;
        case SDL_SCANCODE_F10: return 0x44;
        case SDL_SCANCODE_HOME: return 0x47;
        case SDL_SCANCODE_UP: return 0x48;
        case SDL_SCANCODE_PAGEUP: return 0x49;
        case SDL_SCANCODE_LEFT: return 0x4B;
        case SDL_SCANCODE_RIGHT: return 0x4D;
        case SDL_SCANCODE_END: return 0x4F;
        case SDL_SCANCODE_DOWN: return 0x50;
        case SDL_SCANCODE_PAGEDOWN: return 0x51;
        case SDL_SCANCODE_INSERT: return 0x52;
        case SDL_SCANCODE_DELETE: return 0x53;
        default: return 0;
    }
}

inline uint8_t biosAscii(SDL_Scancode sc, bool shift, bool caps) {
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) {
        char c = char('a' + (int(sc) - int(SDL_SCANCODE_A)));
        if (shift != caps) c = char(c - 32);
        return uint8_t(c);
    }
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_0) {
        static const char plain[] = "1234567890";
        static const char shifted[] = "!@#$%^&*()";
        const int i = int(sc) - int(SDL_SCANCODE_1);
        return uint8_t(shift ? shifted[i] : plain[i]);
    }
    switch (sc) {
        case SDL_SCANCODE_RETURN: return 0x0D;
        case SDL_SCANCODE_ESCAPE: return 0x1B;
        case SDL_SCANCODE_BACKSPACE: return 0x08;
        case SDL_SCANCODE_TAB: return 0x09;
        case SDL_SCANCODE_SPACE: return 0x20;
        case SDL_SCANCODE_MINUS: return shift ? '_' : '-';
        case SDL_SCANCODE_EQUALS: return shift ? '+' : '=';
        case SDL_SCANCODE_LEFTBRACKET: return shift ? '{' : '[';
        case SDL_SCANCODE_RIGHTBRACKET: return shift ? '}' : ']';
        case SDL_SCANCODE_BACKSLASH: return shift ? '|' : '\\';
        case SDL_SCANCODE_SEMICOLON: return shift ? ':' : ';';
        case SDL_SCANCODE_APOSTROPHE: return shift ? '"' : '\'';
        case SDL_SCANCODE_COMMA: return shift ? '<' : ',';
        case SDL_SCANCODE_PERIOD: return shift ? '>' : '.';
        case SDL_SCANCODE_SLASH: return shift ? '?' : '/';
        case SDL_SCANCODE_GRAVE: return shift ? '~' : '`';
        default: return 0;
    }
}

struct Keyboard {
    std::deque<uint16_t> queue;
    bool shift = false, ctrl = false, alt = false, caps = false;

    void onKey(SDL_Scancode sc, bool down, bool repeat) {
        if (sc == SDL_SCANCODE_LSHIFT || sc == SDL_SCANCODE_RSHIFT) {
            shift = down;
            return;
        }
        if (sc == SDL_SCANCODE_LCTRL || sc == SDL_SCANCODE_RCTRL) {
            ctrl = down;
            return;
        }
        if (sc == SDL_SCANCODE_LALT || sc == SDL_SCANCODE_RALT) {
            alt = down;
            return;
        }
        if (sc == SDL_SCANCODE_CAPSLOCK) {
            if (down && !repeat) caps = !caps;
            return;
        }
        if (!down) return;
        const uint8_t scan = biosScan(sc);
        if (!scan) return;
        queue.push_back(uint16_t((scan << 8) | biosAscii(sc, shift, caps)));
        if (queue.size() > 64) queue.pop_front();
    }

    bool pop(uint16_t& v) {
        if (queue.empty()) return false;
        v = queue.front();
        queue.pop_front();
        return true;
    }

    bool peek(uint16_t& v) const {
        if (queue.empty()) return false;
        v = queue.front();
        return true;
    }
};

inline void int16(Keyboard& k, Regs16& r) {
    switch (r.ah()) {
        case 0x00: {
            uint16_t v = 0;
            if (k.pop(v)) r.ax = v;
            else r.ax = 0;  // non-blocking in the native port
            break;
        }
        case 0x01: {
            uint16_t v = 0;
            if (k.peek(v)) {
                r.ax = v;
                r.flags |= 0x40;  // ZF=0
            } else {
                r.ax = 0;
                r.flags &= uint16_t(~0x40);
            }
            break;
        }
        case 0x02:
            r.setAl(uint8_t((k.shift ? 0x03 : 0) | (k.ctrl ? 0x04 : 0) |
                            (k.alt ? 0x08 : 0)));
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// INT 33h - Microsoft mouse.
// ---------------------------------------------------------------------------
struct Mouse {
    int x = VGA_W / 2, y = VGA_H / 2;
    int dx = 0, dy = 0;
    bool left = false, right = false;
    bool visible = true;
};

inline void int33(Mouse& m, Regs16& r) {
    switch (r.ax) {
        case 0x0000:  // reset / install check
            r.ax = 0xFFFF;
            r.bx = 3;
            break;
        case 0x0001:
            m.visible = true;
            break;
        case 0x0002:
            m.visible = false;
            break;
        case 0x0003:  // get position + buttons
            r.bx = uint16_t((m.left ? 1 : 0) | (m.right ? 2 : 0));
            r.cx = uint16_t(std::clamp(m.x, 0, VGA_W - 1));
            r.dx = uint16_t(std::clamp(m.y, 0, VGA_H - 1));
            break;
        case 0x0004:  // set position
            m.x = std::clamp<int>(r.cx, 0, VGA_W - 1);
            m.y = std::clamp<int>(r.dx, 0, VGA_H - 1);
            break;
        case 0x0005:  // button press
            r.ax = uint16_t((m.left ? 1 : 0) | (m.right ? 2 : 0));
            r.bx = 0;
            break;
        case 0x0006:  // button release
            r.ax = 0;
            r.bx = 0;
            break;
        case 0x000B:  // motion counters
            r.cx = uint16_t(m.dx);
            r.dx = uint16_t(m.dy);
            m.dx = m.dy = 0;
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// BIOS timer: 18.2065 Hz (PIT channel 0, 1193182 / 65536).
// ---------------------------------------------------------------------------
struct Clock {
    static constexpr double TICK_HZ = 18.2065;
    uint64_t ticks = 0;
    double accumulator = 0.0;

    void advance(double seconds) {
        accumulator += seconds * TICK_HZ;
        while (accumulator >= 1.0) {
            accumulator -= 1.0;
            ++ticks;
        }
    }
};

// ---------------------------------------------------------------------------
// Port I/O used by the VGA/SB/AdLib code paths.
// ---------------------------------------------------------------------------
struct Ports {
    Video& video;
    bool retrace = false;

    explicit Ports(Video& v) : video(v) {}

    uint8_t in(uint16_t port) {
        switch (port) {
            case 0x3DA:  // input status register 1: toggle retrace bits
                retrace = !retrace;
                return uint8_t(retrace ? 0x09 : 0x00);
            case 0x3C9:  // DAC data (not normally read)
                return 0;
            case 0x60:
                return 0;
            case 0x61:
                return 0x20;
            default:
                return 0xFF;
        }
    }

    void out(uint16_t port, uint8_t value) {
        switch (port) {
            case 0x3C8:  // DAC write index
                video.dacIndex = value;
                video.dacComponent = 0;
                break;
            case 0x3C9:  // DAC data
                video.dacWrite(value);
                break;
            case 0x3C4:
            case 0x3C5:
            case 0x3CE:
            case 0x3CF:
            case 0x3D4:
            case 0x3D5:
                // VGA sequencer / graphics / CRTC registers. For a mode-13h
                // game you usually do not need to emulate these: translate
                // the few writes into Video state if the game uses Mode X.
                break;
            default:
                break;  // SB/AdLib hooks go here (see README)
        }
    }
};

// ---------------------------------------------------------------------------
// SDL2 presentation: virtual framebuffer -> texture -> window.
// ---------------------------------------------------------------------------
class Renderer {
public:
    bool init(const char* title, int scale) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;
        window_ = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED, CANVAS_W * scale,
                                   CANVAS_H * scale, SDL_WINDOW_RESIZABLE);
        renderer_ = SDL_CreateRenderer(
            window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING, CANVAS_W,
                                     CANVAS_H);
        if (!window_ || !renderer_ || !texture_) return false;
        SDL_RenderSetLogicalSize(renderer_, CANVAS_W, CANVAS_H);
        canvas_.resize(CANVAS_W * CANVAS_H);
        loadFont();
        return true;
    }

    ~Renderer() {
        if (texture_) SDL_DestroyTexture(texture_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    void present(Video& video, double timeSeconds) {
        if (video.mode == 0x13) drawVga(video);
        else drawText(video, timeSeconds);
        SDL_UpdateTexture(texture_, nullptr, canvas_.data(),
                          CANVAS_W * int(sizeof(uint32_t)));
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(renderer_);
    }

private:
    void loadFont() {
        char* base = SDL_GetBasePath();
        const std::string path = std::string(base ? base : "") + "assets/font8x16.bin";
        if (base) SDL_free(base);
        FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) {
            std::fprintf(stderr,
                         "[dos] font not found: %s (using fallback glyphs)\n",
                         path.c_str());
            return;
        }
        const size_t n = std::fread(font_.data(), 1, font_.size(), f);
        std::fclose(f);
        fontLoaded_ = (n == font_.size());
    }

    void drawText(Video& video, double t) {
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
        // Text-mode cursor: underline at the current cell.
        if (video.cursorVisible && blink && video.cursorX < TEXT_COLS &&
            video.cursorY < TEXT_ROWS) {
            const uint32_t color = video.argb[0x0F];
            for (int row = 14; row < 16; ++row)
                for (int col = 0; col < 8; ++col)
                    canvas_[(video.cursorY * 16 + row) * CANVAS_W +
                            video.cursorX * 8 + col] = color;
        }
    }

    void drawVga(Video& video) {
        for (int y = 0; y < VGA_H; ++y) {
            for (int x = 0; x < VGA_W; ++x) {
                const uint32_t c = video.argb[video.vga[y * VGA_W + x]];
                const size_t o0 = size_t(y * 2) * CANVAS_W + x * 2;
                canvas_[o0] = c;
                canvas_[o0 + 1] = c;
                canvas_[o0 + CANVAS_W] = c;
                canvas_[o0 + CANVAS_W + 1] = c;
            }
        }
    }

    static void makeFallbackGlyph(uint8_t ch, std::array<uint8_t, 16>& out) {
        out.fill(0);
        out[2] = 0xFF;
        out[13] = 0xFF;
        for (int row = 3; row < 13; ++row) {
            uint8_t bits = uint8_t(0x81 | (ch & 0x7E));
            if ((ch >> (row % 6)) & 1) bits ^= 0x7E;
            out[row] = bits;
        }
    }

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    std::vector<uint32_t> canvas_;
    std::array<uint8_t, 256 * 16> font_{};
    bool fontLoaded_ = false;
};

}  // namespace dos

// ===========================================================================
// Presentation: the original writes CP437 characters into B800:0000; the port
// writes the same characters into dos::Video::text and the Renderer turns that
// into pixels with the font ripped from the game's own VGA BIOS.
//
// Screen geometry recovered from FUN_1000_1920 (rules at rows 1/7/15/24 and a
// split at column 39) and FUN_1000_258a (hand label at row 16, cards at row 19,
// first column 1, selected cards raised by one row).
// ===========================================================================
namespace ui {

using buraco::Card;
using buraco::Game;
using buraco::Meld;

constexpr int COLS = dos::TEXT_COLS;
constexpr int ROWS = dos::TEXT_ROWS;

constexpr uint8_t FIELD = 0x2F;     // bright white on green
constexpr uint8_t DIM = 0x2A;       // light green on green
constexpr uint8_t CARD = 0x70;      // black on light grey
constexpr uint8_t CARD_RED = 0x74;  // red on light grey
constexpr uint8_t CARD_SEL = 0x1F;  // white on blue
constexpr uint8_t BACK = 0x74;      // red on light grey (▒ hatch)
constexpr uint8_t WILD = 0x2E;      // yellow on green

constexpr int ROW_MSG = 0;
constexpr int ROW_RULE1 = 1;
constexpr int ROW_RULE2 = 7;
constexpr int ROW_RULE3 = 15;
constexpr int ROW_RULE4 = 24;
constexpr int COL_SPLIT = 39;
constexpr int ROW_PILE = 2;
constexpr int COL_MORTO = 1;
constexpr int COL_MESA = 11;
constexpr int ROW_LABEL = 16;
constexpr int ROW_CURSOR = 17;
constexpr int ROW_HAND = 19;
constexpr int ROW_MELD = 9;
constexpr int COL_MELD_L = 1;
constexpr int COL_MELD_R = 41;

struct UiState {
    int cursor = 0;
    int meldScroll = 0;   // first meld row shown in the table panel
    std::vector<bool> selected;
    bool help = false;
    bool status = false;
    // Pending "grow an existing meld or lay a new one?" question.  While it is
    // set the selected cards are kept aside and only A/N/ESC are accepted.
    bool layChoice = false;
    int layTarget = -1;
    std::vector<Card> layCards;
};

inline void put(dos::Video& v, int col, int row, uint8_t ch, uint8_t attr) {
    if (col < 0 || col >= COLS || row < 0 || row >= ROWS) return;
    const int o = (row * COLS + col) * 2;
    v.text[o] = ch;
    v.text[o + 1] = attr;
}

inline void puts(dos::Video& v, int col, int row, const std::string& s, uint8_t attr) {
    for (char c : s) put(v, col++, row, uint8_t(c), attr);
}

inline void hrule(dos::Video& v, int row, uint8_t ch, uint8_t attr) {
    for (int c = 0; c < COLS; ++c) put(v, c, row, ch, attr);
}

inline void clear(dos::Video& v) {
    for (int i = 0; i < COLS * ROWS * 2; i += 2) {
        v.text[i] = ' ';
        v.text[i + 1] = FIELD;
    }
}

// One 6x5 text-cell card, exactly like FUN_1000_1dfa.
inline void drawCard(dos::Video& v, const Card& c, int col, int row, bool faceUp,
                     bool selected = false) {
    const uint8_t base = selected ? CARD_SEL : CARD;
    put(v, col, row, 0xDA, base);  // ┌
    for (int i = 1; i <= 4; ++i) put(v, col + i, row, 0xC4, base);
    put(v, col + 5, row, 0xBF, base);  // ┐
    for (int r = 1; r <= 3; ++r) {
        put(v, col, row + r, 0xB3, base);
        put(v, col + 5, row + r, 0xB3, base);
        for (int i = 1; i <= 4; ++i)
            put(v, col + i, row + r, faceUp ? ' ' : 0xB1,
                faceUp ? base : (selected ? CARD_SEL : BACK));
    }
    put(v, col, row + 4, 0xC0, base);  // └
    for (int i = 1; i <= 4; ++i) put(v, col + i, row + 4, 0xC4, base);
    put(v, col + 5, row + 4, 0xD9, base);  // ┘
    if (!faceUp) return;

    const bool red = buraco::isRedSuit(c);
    const uint8_t fg = selected ? CARD_SEL : (red ? CARD_RED : CARD);
    put(v, col + 1, row + 1, uint8_t(buraco::rankGlyph(c)), fg);
    if (c.rank == 10) put(v, col + 2, row + 1, '0', fg);
    put(v, col + 1, row + 2, uint8_t(buraco::suitGlyph(c)), fg);
}

struct FanResult {
    int endCol = 0;
    std::vector<int> columns;  // starting column of every drawn card
};

// Draws a fanned row of cards.  `maxCol` is the last usable column.
inline FanResult drawFan(dos::Video& v, const std::vector<Card>& cards, int col, int row,
                         bool faceUp, int maxCol, const std::vector<bool>* selected) {
    FanResult out;
    int x = col;
    for (size_t i = 0; i < cards.size(); ++i) {
        const int step = buraco::cardWidth(cards[i]);
        if (x + step > maxCol + 1) break;
        const bool sel = selected && i < selected->size() && (*selected)[i];
        out.columns.push_back(x);
        drawCard(v, cards[i], x, row - (sel ? 1 : 0), faceUp, sel);
        x += step;
    }
    out.endCol = x;
    return out;
}

// Compact meld line: the suit once, then the ranks in order.  Wildcards keep
// their own printed rank+suit and are drawn in yellow.
inline void drawMeldLine(dos::Video& v, const Meld& m, int col, int row, int maxCol) {
    if (m.empty()) return;
    int x = col;
    put(v, x++, row, uint8_t(buraco::suitGlyphFor(m.suit)), FIELD);
    put(v, x++, row, ' ', FIELD);
    for (int slot = 1; slot <= 15; ++slot) {
        const Card& c = m.slot[slot];
        if (!c.valid()) continue;
        if (x + 4 > maxCol) {
            puts(v, x, row, "...", DIM);
            return;
        }
        const bool wild = m.wild[slot];
        const uint8_t fg = wild ? WILD : FIELD;
        if (wild) {
            put(v, x++, row, uint8_t(buraco::rankGlyph(c)), fg);
            if (c.rank == 10) put(v, x++, row, '0', fg);
            put(v, x++, row, uint8_t(buraco::suitGlyph(c)), fg);
        } else {
            put(v, x++, row, uint8_t(buraco::rankGlyph(c)), fg);
            if (c.rank == 10) put(v, x++, row, '0', fg);
        }
        put(v, x++, row, ' ', FIELD);
    }
    if (m.canastra()) {
        const std::string tag = m.dirty ? "c+100" : "c+200";
        puts(v, x + 1, row, tag, m.dirty ? WILD : DIM);
    }
}

inline void drawPiles(dos::Video& v, const Game& g) {
    // Morto: the two halves are stacked with a two-column offset, so the second
    // back's frame sits on top of the first (see the morto box in FUN_1000_1920).
    const int halves = (g.mortoRemaining() + buraco::MORTO_HALF - 1) / buraco::MORTO_HALF;
    for (int i = 0; i < halves; ++i) drawCard(v, Card{}, COL_MORTO + i * 2, ROW_PILE, false);

    // Discard pile ("a mesa"): the original fans every card left to right.
    const auto& pile = g.discardPile();
    if (!pile.empty()) {
        const int maxCards = 26;
        std::vector<Card> shown(pile.end() - std::min<size_t>(pile.size(), maxCards), pile.end());
        drawFan(v, shown, COL_MESA, ROW_PILE, true, 70, nullptr);
    }
}

inline void drawMelds(dos::Video& v, const Game& g, const UiState& ui) {
    static const char* kHeader[2] = {"SEUS JOGOS", "MEUS JOGOS"};
    const int col[2] = {COL_MELD_L, COL_MELD_R};
    const int maxCol[2] = {37, 78};
    const int rows = ROW_RULE3 - ROW_MELD;
    for (int seat = 0; seat < 2; ++seat) {
        const auto& melds = g.player(seat).melds;
        int total = 0;
        for (const Meld& m : melds)
            if (!m.empty()) ++total;
        const int start = std::clamp(ui.meldScroll, 0, std::max(0, total - rows));

        if (total > rows) {
            char header[40];
            std::snprintf(header, sizeof header, "%s %d-%d/%d", kHeader[seat], start + 1,
                          std::min(total, start + rows), total);
            puts(v, col[seat], ROW_RULE2 + 1, header, DIM);
        } else {
            puts(v, col[seat], ROW_RULE2 + 1, kHeader[seat], DIM);
        }

        int row = ROW_MELD;
        int shown = 0;
        for (size_t i = size_t(start); i < melds.size() && shown < rows; ++i) {
            if (melds[i].empty()) continue;
            drawMeldLine(v, melds[i], col[seat], row, maxCol[seat]);
            ++row;
            ++shown;
        }
        if (total == 0) puts(v, col[seat], ROW_MELD, "nenhum jogo", DIM);
    }
    // Scores between the two headers so they never sit on top of a meld line.
    char buf[64];
    std::snprintf(buf, sizeof buf, "%d  x  %d", g.player(0).total, g.player(1).total);
    puts(v, 30, ROW_RULE2 + 1, buf, DIM);
}

inline void drawHand(dos::Video& v, const Game& g, const UiState& ui) {
    // During the scoring screen the human hand stays on the table; while the
    // AI is playing its hand is shown face-down under "Minha mao.".
    const bool scoring = g.phase() == buraco::Phase::HandOver ||
                         g.phase() == buraco::Phase::MatchOver;
    const int seat = scoring ? 0 : g.current();
    const auto& p = g.player(seat);
    puts(v, 1, ROW_LABEL, seat == 0 ? "Sua mao." : "Minha mao.", FIELD);

    char buf[64];
    int pts = 0;
    for (const Card& c : p.hand) pts += buraco::cardPoints(c);
    std::snprintf(buf, sizeof buf, "cartas: %2d   pontos: %3d", int(p.hand.size()), pts);
    puts(v, 50, ROW_LABEL, buf, DIM);

    // Never reveal the opponent's cards: the AI hand is only ever drawn from
    // the back.  (PageUp used to toggle a peek, which made it too easy to see
    // the CPU's cards by accident.)
    const bool faceUp = seat == 0;

    // A hand bought from the table can grow past what fits in 78 columns.
    // Show a window that always contains the cursor; moving left/right scrolls
    // it, so every card stays reachable.
    int first = 0;
    int last = int(p.hand.size()) - 1;
    if (seat == 0 && !p.hand.empty()) {
        const int cur = std::clamp(ui.cursor, 0, last);
        int used = buraco::cardWidth(p.hand[cur]);
        first = cur;
        while (first > 0) {
            const int w = buraco::cardWidth(p.hand[first - 1]);
            if (used + w > 78) break;
            used += w;
            --first;
        }
        last = cur;
        while (last < int(p.hand.size()) - 1) {
            const int w = buraco::cardWidth(p.hand[last + 1]);
            if (used + w > 78) break;
            used += w;
            ++last;
        }
    }

    std::vector<Card> visible;
    if (!p.hand.empty()) visible.assign(p.hand.begin() + first, p.hand.begin() + last + 1);
    std::vector<bool> visibleSel;
    const std::vector<bool>* selPtr = nullptr;
    if (seat == 0) {
        visibleSel.assign(visible.size(), false);
        for (size_t i = 0; i < visible.size(); ++i) {
            const size_t src = size_t(first) + i;
            if (src < ui.selected.size()) visibleSel[i] = ui.selected[src];
        }
        selPtr = &visibleSel;
    }

    FanResult fan = drawFan(v, visible, 1, ROW_HAND, faceUp, 78, selPtr);

    if (seat == 0 && !p.hand.empty()) {
        const int idx = std::clamp(ui.cursor, 0, int(p.hand.size()) - 1) - first;
        if (idx >= 0 && idx < int(fan.columns.size()))
            put(v, fan.columns[idx] + 1, ROW_CURSOR, 0x1F, FIELD);  // ▼ above the cursor card
    }
    if (first > 0) put(v, 0, ROW_HAND, '<', DIM);
    if (last < int(p.hand.size()) - 1) put(v, 79, ROW_HAND, '>', DIM);
}

inline void drawStatus(dos::Video& v, const Game& g) {
    const int r0 = 9, r1 = 14, c0 = 20, c1 = 60;
    for (int r = r0; r <= r1; ++r)
        for (int c = c0; c <= c1; ++c) put(v, c, r, ' ', FIELD);
    put(v, c0, r0, 0xC9, FIELD);
    for (int c = c0 + 1; c < c1; ++c) put(v, c, r0, 0xCD, FIELD);
    put(v, c1, r0, 0xBB, FIELD);
    for (int r = r0 + 1; r < r1; ++r) {
        put(v, c0, r, 0xBA, FIELD);
        put(v, c1, r, 0xBA, FIELD);
    }
    put(v, c0, r1, 0xC8, FIELD);
    for (int c = c0 + 1; c < c1; ++c) put(v, c, r1, 0xCD, FIELD);
    put(v, c1, r1, 0xBC, FIELD);

    puts(v, c0 + 3, r0 + 1, "SITUACAO GERAL", FIELD);
    char buf[80];
    std::snprintf(buf, sizeof buf, "%-8s  pontos %5d", g.player(0).name.c_str(),
                  g.player(0).total);
    puts(v, c0 + 3, r0 + 2, buf, FIELD);
    std::snprintf(buf, sizeof buf, "%-8s  pontos %5d", g.player(1).name.c_str(),
                  g.player(1).total);
    puts(v, c0 + 3, r0 + 3, buf, FIELD);
    std::snprintf(buf, sizeof buf, "mao %d   meta %d", g.handNumber(), g.targetScore());
    puts(v, c0 + 3, r0 + 4, buf, DIM);
}

inline void drawHandOver(dos::Video& v, const Game& g) {
    const int r0 = 8, r1 = 15, c0 = 0, c1 = 79;
    for (int r = r0; r <= r1; ++r)
        for (int c = c0; c <= c1; ++c) put(v, c, r, ' ', FIELD);
    put(v, c0, r0, 0xC9, FIELD);
    for (int c = c0 + 1; c < c1; ++c) put(v, c, r0, 0xCD, FIELD);
    put(v, c1, r0, 0xBB, FIELD);
    for (int r = r0 + 1; r < r1; ++r) {
        put(v, c0, r, 0xBA, FIELD);
        put(v, c1, r, 0xBA, FIELD);
    }
    put(v, c0, r1, 0xC8, FIELD);
    for (int c = c0 + 1; c < c1; ++c) put(v, c, r1, 0xCD, FIELD);
    put(v, c1, r1, 0xBC, FIELD);

    char buf[80];
    puts(v, c0 + 4, r0 + 1, "Vamos contar os pontos.", FIELD);
    std::snprintf(buf, sizeof buf, "%-8s %6d", g.player(0).name.c_str(),
                  g.player(0).lastHandPoints);
    puts(v, c0 + 4, r0 + 3, buf, FIELD);
    std::snprintf(buf, sizeof buf, "%-8s %6d", g.player(1).name.c_str(),
                  g.player(1).lastHandPoints);
    puts(v, c0 + 4, r0 + 4, buf, FIELD);
    std::snprintf(buf, sizeof buf, "total %d x %d", g.player(0).total, g.player(1).total);
    puts(v, c0 + 4, r0 + 5, buf, DIM);
    puts(v, c0 + 4, r0 + 6, "ENTER para continuar", WILD);
}

inline void drawHelp(dos::Video& v) {
    static const char* kLines[] = {
        "Vamos jogar buraco ?",
        "",
        "Durante o jogo, voce pode responder minhas perguntas com \"S\" ou \"N\".",
        "As flechas para esquerda e direita servem para escolher as suas cartas.",
        "Quando voce tiver escolhido, aperte a barra de espacos para que eu",
        "saiba qual carta voce escolheu. Se voce nao quizer mais esta carta,",
        "aperte a barra novamente. Para baixar um jogo novo, escolha pelo menos",
        "tres cartas e aperte ENTER. Para aumentar um jogo, escolha uma ou mais",
        "cartas. Para descartar, aperte ENTER sem ter escolhido nenhuma carta.",
        "Se ja houver um jogo do mesmo naipe, eu pergunto se voce quer aumentar",
        "o jogo ou baixar um novo.",
        "",
        "As flechas para cima e para baixo mostram os jogos que estao na mesa.",
        "Os jogos dos dois jogadores ficam sempre visiveis.",
        "Para ver esta tela, aperte F1.",
        "Para ver a situacao geral do jogo, aperte F3.",
        "Para gravar o jogo aperte F2; para restaurar, F5.",
        "",
        "Boa sorte!",
        "",
        "               ...agora aperte <ENTER>",
    };
    for (int r = 1; r <= 23; ++r)
        for (int c = 1; c <= 78; ++c) put(v, c, r, ' ', FIELD);
    put(v, 1, 1, 0xC9, FIELD);
    for (int c = 2; c < 78; ++c) put(v, c, 1, 0xCD, FIELD);
    put(v, 78, 1, 0xBB, FIELD);
    for (int r = 2; r < 23; ++r) {
        put(v, 1, r, 0xBA, FIELD);
        put(v, 78, r, 0xBA, FIELD);
    }
    put(v, 1, 23, 0xC8, FIELD);
    for (int c = 2; c < 78; ++c) put(v, c, 23, 0xCD, FIELD);
    put(v, 78, 23, 0xBC, FIELD);

    int row = 2;
    for (const char* line : kLines) {
        puts(v, 3, row, line, FIELD);
        ++row;
    }
    puts(v, 1, 0, "BURACO - ajuda", DIM);
}

inline void draw(dos::Video& v, const Game& g, const UiState& ui) {
    clear(v);
    hrule(v, ROW_RULE1, 0xC4, FIELD);
    hrule(v, ROW_RULE2, 0xC4, FIELD);
    hrule(v, ROW_RULE3, 0xC4, FIELD);
    hrule(v, ROW_RULE4, 0xC4, FIELD);
    put(v, COL_SPLIT, ROW_RULE2, 0xD2, FIELD);
    for (int r = ROW_RULE2 + 1; r < ROW_RULE3; ++r) put(v, COL_SPLIT, r, 0xBA, FIELD);
    put(v, COL_SPLIT, ROW_RULE3, 0xD0, FIELD);

    puts(v, 0, ROW_MSG, g.message(), FIELD);

    drawPiles(v, g);
    drawMelds(v, g, ui);
    drawHand(v, g, ui);

    if (g.phase() == buraco::Phase::HandOver) drawHandOver(v, g);
    if (ui.status) drawStatus(v, g);
    if (ui.help) drawHelp(v);
}

}  // namespace ui

// ---------------------------------------------------------------------------
// Save / restore
// ---------------------------------------------------------------------------
namespace save {

// Default is a file next to the working directory; --save overrides it (used by
// the headless regression scripts so they never clobber a real save).
std::string& path() {
    static std::string p = "buraco.sav";
    return p;
}

}  // namespace save

// ---------------------------------------------------------------------------
// Scripted key names (used by the headless regression mode).
// ---------------------------------------------------------------------------
static SDL_Keycode keyFromToken(const std::string& t) {
    if (t == "ENTER" || t == "RETURN") return SDLK_RETURN;
    if (t == "SPACE") return SDLK_SPACE;
    if (t == "LEFT") return SDLK_LEFT;
    if (t == "RIGHT") return SDLK_RIGHT;
    if (t == "UP") return SDLK_UP;
    if (t == "DOWN") return SDLK_DOWN;
    if (t == "ESC" || t == "ESCAPE") return SDLK_ESCAPE;
    if (t == "F1") return SDLK_F1;
    if (t == "F2") return SDLK_F2;
    if (t == "F3") return SDLK_F3;
    if (t == "F4") return SDLK_F4;
    if (t == "F5") return SDLK_F5;
    if (t == "PGUP" || t == "PAGEUP") return SDLK_PAGEUP;
    if (t == "Y") return SDLK_y;
    if (t == "S" || t == "SIM") return SDLK_s;
    if (t == "N" || t == "NAO") return SDLK_n;
    if (t.size() == 1) return SDL_Keycode(std::tolower(uint8_t(t[0])));
    return 0;
}

static void dumpScreen(const dos::Video& v, std::FILE* f, const char* label) {
    std::fprintf(f, "# %s\n", label);
    for (int r = 0; r < dos::TEXT_ROWS; ++r) {
        for (int c = 0; c < dos::TEXT_COLS; ++c) std::fputc(v.text[(r * dos::TEXT_COLS + c) * 2], f);
        std::fputc('\n', f);
    }
}

// Machine-readable game state next to every screen, so a replay log can be
// asserted without OCR: suits are 1..4 (♦♠♥♣) and 5 for jokers, ranks 1..13.
static std::string stateLine(const buraco::Game& g) {
    char buf[512];
    std::string s;
    std::snprintf(buf, sizeof buf, "# state phase=%d cur=%d rng=%u hand=", int(g.phase()),
                  g.current(), unsigned(g.rng()));
    s += buf;
    for (const buraco::Card& c : g.player(0).hand) {
        std::snprintf(buf, sizeof buf, "%u/%u ", unsigned(c.suit), unsigned(c.rank));
        s += buf;
    }
    s += "ai=";
    for (const buraco::Card& c : g.player(1).hand) {
        std::snprintf(buf, sizeof buf, "%u/%u ", unsigned(c.suit), unsigned(c.rank));
        s += buf;
    }
    std::snprintf(buf, sizeof buf, "| morto=%d monte=%zu mesa=%zu", g.mortoRemaining(),
                  g.monte().size(), g.discardPile().size());
    s += buf;
    for (int seat = 0; seat < 2; ++seat) {
        std::snprintf(buf, sizeof buf, "| p%d melds:", seat);
        s += buf;
        for (const buraco::Meld& m : g.player(seat).melds) {
            if (m.empty()) continue;
            std::snprintf(buf, sizeof buf, " s%u n%d v%d[", unsigned(m.suit), m.size(), m.value());
            s += buf;
            for (const buraco::Card& c : m.cards()) {
                std::snprintf(buf, sizeof buf, "%u,", unsigned(c.id));
                s += buf;
            }
            s += "]";
        }
    }
    std::snprintf(buf, sizeof buf, "| pts %d x %d| h%d\n", g.player(0).total,
                  g.player(1).total, g.handNumber());
    s += buf;
    return s;
}

// Strict dump used by --savetest: unlike stateLine it also prints physical
// card ids and meld wildcard flags, so a save/load round-trip that swaps a
// duplicate card cannot go unnoticed.
static std::string strictLine(const buraco::Game& g) {
    std::string s = stateLine(g);
    char buf[64];
    for (int seat = 0; seat < 2; ++seat) {
        std::snprintf(buf, sizeof buf, "H%d:", seat);
        s += buf;
        for (const buraco::Card& c : g.player(seat).hand) {
            std::snprintf(buf, sizeof buf, " %u", unsigned(c.id));
            s += buf;
        }
        for (const buraco::Meld& m : g.player(seat).melds) {
            std::snprintf(buf, sizeof buf, " [s%u d%d", unsigned(m.suit), m.dirty ? 1 : 0);
            s += buf;
            for (int i = 1; i <= 15; ++i) {
                if (!m.slot[i].valid()) continue;
                std::snprintf(buf, sizeof buf, " %d=%u%s", i, unsigned(m.slot[i].id),
                              m.wild[i] ? "w" : "");
                s += buf;
            }
            s += "]";
        }
    }
    s += '\n';
    return s;
}

static void dumpState(const buraco::Game& g, std::FILE* f) {
    const std::string s = stateLine(g);
    std::fwrite(s.data(), 1, s.size(), f);
}

// Sanity check for the card accounting: every one of the 108 cards must be in
// exactly one place (a hand, a meld, the morto, the monte or the table).
static bool checkInvariants(const buraco::Game& g, std::string& err) {
    std::array<int, buraco::DECK_SIZE + 1> seen{};
    auto claim = [&](const buraco::Card& c, const char* where) {
        if (!c.valid() || c.id > buraco::DECK_SIZE) {
            err = std::string("invalid card id in ") + where;
            return false;
        }
        if (++seen[c.id] != 1) {
            err = std::string("duplicated card ") + std::to_string(int(c.id)) + " in " + where;
            return false;
        }
        return true;
    };
    for (int seat = 0; seat < 2; ++seat) {
        for (const buraco::Card& c : g.player(seat).hand)
            if (!claim(c, "hand")) return false;
        for (const buraco::Meld& m : g.player(seat).melds) {
            int lo = m.lowSlot(), hi = m.highSlot();
            if (lo == 0) {
                err = "empty meld in list";
                return false;
            }
            for (int i = lo; i <= hi; ++i)
                if (!m.slot[i].valid()) {
                    err = "meld with a hole";
                    return false;
                }
            for (const buraco::Card& c : m.cards())
                if (!claim(c, "meld")) return false;
        }
    }
    for (const buraco::Card& c : g.morto()) if (!claim(c, "morto")) return false;
    for (const buraco::Card& c : g.monte()) if (!claim(c, "monte")) return false;
    for (const buraco::Card& c : g.discardPile()) if (!claim(c, "mesa")) return false;

    int total = 0;
    for (int id = 1; id <= buraco::DECK_SIZE; ++id) total += seen[id];
    if (total != buraco::DECK_SIZE) {
        err = "card count " + std::to_string(total) + " != 108";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Entry point
//
//   buraco.exe                          normal SDL2 window
//   buraco.exe --script k.txt --out s.txt
//                                       headless: replay key tokens, dump the
//                                       80x25 text screen after every step
// ---------------------------------------------------------------------------

// Fresh 32-bit seed for a new match.  SDL_GetTicks() is nearly constant at
// startup (the tick counter only starts when SDL initialises), which used to
// make every deal identical; mix the wall clock and random_device instead.
static uint32_t makeSeed() {
    const uint64_t t =
        uint64_t(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::random_device rd;
    uint64_t v = t ^ (uint64_t(rd()) << 32) ^ uint64_t(rd());
    v += 0x9E3779B97F4A7C15ull;   // splitmix64 finaliser
    v = (v ^ (v >> 30)) * 0xBF58476D1CE4E5B9ull;
    v = (v ^ (v >> 27)) * 0x94D049BB133111EBull;
    v ^= v >> 31;
    const uint32_t s = uint32_t(v ^ (v >> 32));
    return s ? s : 0x9E3779B9u;
}

int main(int argc, char** argv) {
    std::string scriptPath;
    std::string outPath = "screen.txt";
    uint32_t seed = 0;
    int autoSteps = 0;
    int checkSteps = 0;
    int saveTestSteps = 0;
    bool guiAfterAuto = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--script" && i + 1 < argc) scriptPath = argv[++i];
        else if (a == "--out" && i + 1 < argc) outPath = argv[++i];
        else if (a == "--seed" && i + 1 < argc) seed = uint32_t(std::strtoul(argv[++i], nullptr, 10));
        else if (a == "--auto" && i + 1 < argc) autoSteps = std::atoi(argv[++i]);
        else if (a == "--check" && i + 1 < argc) checkSteps = std::atoi(argv[++i]);
        else if (a == "--savetest" && i + 1 < argc) saveTestSteps = std::atoi(argv[++i]);
        else if (a == "--save" && i + 1 < argc) save::path() = argv[++i];
        else if (a == "--gui") guiAfterAuto = true;
    }
    bool headless = (!scriptPath.empty() || autoSteps > 0) && !guiAfterAuto;
    // Every batch mode (script, auto, check, savetest) keeps the historical
    // default seed so failures are reproducible; only the windowed game picks
    // a fresh seed per match.
    const bool batch = headless || checkSteps > 0 || saveTestSteps > 0;

    dos::Video video;
    video.setMode(0x03);
    video.cursorVisible = false;  // the port draws its own marker on row 17

    buraco::Game game;
    ui::UiState ui;
    game.newMatch("JOGADOR", seed ? seed : (batch ? 1234u : makeSeed()));
    bool started = false;
    bool running = true;

    auto resetSelection = [&]() {
        ui.selected.assign(game.player(0).hand.size(), false);
        ui.cursor = std::clamp(ui.cursor, 0, std::max(0, int(game.player(0).hand.size()) - 1));
        ui.layChoice = false;
        ui.layTarget = -1;
        ui.layCards.clear();
    };

    auto key = [&](SDL_Keycode sym) {
        using buraco::Phase;
        // Persistence works in every screen, including before the deal, so a
        // match can be restored without starting a fresh one.
        if (sym == SDLK_F2) {
            if (game.saveTo(save::path()))
                game.setMessage("Jogo gravado em " + save::path() + ".");
            else
                game.setMessage("Nao consegui gravar o jogo.");
            return;
        }
        if (sym == SDLK_F5) {
            if (game.loadFrom(save::path())) {
                started = true;
                ui.help = false;
                resetSelection();
                game.setMessage("Jogo restaurado.");
            } else {
                game.setMessage("Nao consegui restaurar o jogo.");
            }
            return;
        }
        if (!started) {
            if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
                started = true;
                game.newHand();
                resetSelection();
            }
            return;
        }
        if (ui.help) {
            if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER || sym == SDLK_F1 ||
                sym == SDLK_ESCAPE)
                ui.help = false;
            return;
        }

        const int handSize = int(game.player(0).hand.size());

        switch (sym) {
            case SDLK_ESCAPE:
                if (std::any_of(ui.selected.begin(), ui.selected.end(),
                                [](bool b) { return b; })) {
                    std::fill(ui.selected.begin(), ui.selected.end(), false);
                } else {
                    running = false;
                }
                return;
            case SDLK_F1:
                ui.help = true;
                return;
            case SDLK_F3:
                ui.status = !ui.status;
                return;
            case SDLK_F4:
                ui.status = false;
                return;
            default:
                break;
        }

        if (game.phase() == Phase::HandOver) {
            if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
                game.continueAfterHand();
                resetSelection();
            }
            return;
        }
        if (game.phase() == Phase::MatchOver) {
            if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
                game.newMatch("JOGADOR", seed ? seed : makeSeed());
                resetSelection();
            }
            return;
        }
        if (game.current() != 0) return;  // the AI is playing

        // "Grow the existing meld or lay a new one?" question.
        if (ui.layChoice) {
            if (sym == SDLK_a || sym == SDLK_s || sym == SDLK_y) {
                const std::string err = game.layMeld(ui.layCards, ui.layTarget);
                if (!err.empty()) game.setMessage(err);
                resetSelection();
            } else if (sym == SDLK_n) {
                const std::string err = game.layMeld(ui.layCards, buraco::Game::kNewMeld);
                if (!err.empty()) game.setMessage(err);
                resetSelection();
            } else if (sym == SDLK_ESCAPE) {
                // Keep the cards selected so the player can adjust them.
                ui.layChoice = false;
                ui.layTarget = -1;
                ui.layCards.clear();
                game.announceTurn();
            }
            return;
        }

        switch (sym) {
            case SDLK_LEFT:
                ui.cursor = std::max(0, ui.cursor - 1);
                return;
            case SDLK_RIGHT:
                ui.cursor = std::min(std::max(0, handSize - 1), ui.cursor + 1);
                return;
            case SDLK_UP:
            case SDLK_DOWN: {
                const int rows = ui::ROW_RULE3 - ui::ROW_MELD;
                int count = 0;
                for (int seat = 0; seat < 2; ++seat) {
                    int n = 0;
                    for (const auto& m : game.player(seat).melds)
                        if (!m.empty()) ++n;
                    count = std::max(count, n);
                }
                const int maxScroll = std::max(0, count - rows);
                ui.meldScroll = std::clamp(ui.meldScroll + (sym == SDLK_DOWN ? 1 : -1), 0,
                                           maxScroll);
                return;
            }
            case SDLK_SPACE:
                if (ui.cursor >= 0 && ui.cursor < int(ui.selected.size()))
                    ui.selected[ui.cursor] = !ui.selected[ui.cursor];
                return;
            case SDLK_s:
            case SDLK_y:
                if (game.phase() == Phase::Draw) {
                    game.draw(true);
                    resetSelection();
                }
                return;
            case SDLK_n:
                if (game.phase() == Phase::Draw) {
                    game.draw(false);
                    resetSelection();
                }
                return;
            case SDLK_RETURN:
            case SDLK_KP_ENTER: {
                if (game.phase() == Phase::Draw) {
                    game.draw(false);
                    resetSelection();
                    return;
                }
                if (game.phase() == Phase::Play) {
                    std::vector<buraco::Card> chosen;
                    for (size_t i = 0; i < ui.selected.size() && i < game.player(0).hand.size();
                         ++i)
                        if (ui.selected[i]) chosen.push_back(game.player(0).hand[i]);
                    if (chosen.empty()) {
                        game.requestDiscard();
                        return;
                    }
                    const buraco::Game::LayOptions opts = game.layOptions(chosen);
                    if (opts.canCreate && !opts.targets.empty()) {
                        // Both readings are legal: let the player decide.
                        ui.layChoice = true;
                        ui.layTarget = opts.targets.front();
                        ui.layCards = chosen;
                        game.setMessage("Aumentar o jogo (A) ou baixar um novo (N)?");
                        return;
                    }
                    const int target = !opts.targets.empty()
                                           ? opts.targets.front()
                                           : (opts.canCreate ? buraco::Game::kNewMeld : -1);
                    const std::string err = game.layMeld(chosen, target);
                    if (!err.empty()) game.setMessage(err);
                    resetSelection();
                    return;
                }
                if (game.phase() == Phase::Discard) {
                    const auto& hand = game.player(0).hand;
                    if (hand.empty()) return;
                    const int idx = std::clamp(ui.cursor, 0, int(hand.size()) - 1);
                    const buraco::Card card = hand[idx];
                    game.discardCard(card);
                    resetSelection();
                    return;
                }
                return;
            }
            default:
                return;
        }
    };

    // ---- self-play for a demo / screenshot, then hand over to the window ----
    if (guiAfterAuto && autoSteps > 0) {
        resetSelection();
        for (int i = 0; i < autoSteps; ++i) {
            if (game.matchOver()) break;
            if (game.phase() == buraco::Phase::HandOver) {
                game.continueAfterHand();
                continue;
            }
            game.autoStep();
        }
        started = true;
        game.announceTurn();
    }

    // ---- save/restore round-trip ------------------------------------------
    if (saveTestSteps > 0) {
        std::FILE* out = std::fopen(outPath.c_str(), "wb");
        if (!out) {
            std::fprintf(stderr, "cannot write %s\n", outPath.c_str());
            return 1;
        }
        auto advance = [](buraco::Game& g, int steps) {
            for (int i = 0; i < steps; ++i) {
                if (g.matchOver()) break;
                if (g.phase() == buraco::Phase::HandOver) {
                    g.continueAfterHand();
                    continue;
                }
                g.autoStep();
            }
        };

        buraco::Game a;
        a.newMatch("JOGADOR", seed ? seed : 1234u);
        advance(a, saveTestSteps);
        const std::string before = strictLine(a);
        bool ok = a.saveTo(save::path());
        buraco::Game b;
        ok = ok && b.loadFrom(save::path());
        ok = ok && strictLine(b) == before;
        std::string detail = "roundtrip";

        // A faithful load must serialise back to exactly the same file: this
        // catches fields that saveTo writes but loadFrom forgets.
        if (ok) {
            b.saveTo(outPath + ".reload.sav");
            std::ifstream f1(save::path(), std::ios::binary);
            std::ifstream f2(outPath + ".reload.sav", std::ios::binary);
            const std::string s1((std::istreambuf_iterator<char>(f1)),
                                 std::istreambuf_iterator<char>());
            const std::string s2((std::istreambuf_iterator<char>(f2)),
                                 std::istreambuf_iterator<char>());
            if (s1 != s2) {
                ok = false;
                detail = "save/load is lossy";
            }
        }

        if (ok) {
            advance(a, 400);
            advance(b, 400);
            const std::string la = strictLine(a);
            const std::string lb = strictLine(b);
            if (la != lb) {
                ok = false;
                detail = "diverged after 400 more steps";
                // Point at the first visible difference; the two states are
                // saved next to the report so the split can be reproduced.
                size_t d = 0;
                while (d < la.size() && d < lb.size() && la[d] == lb[d]) ++d;
                const size_t from = d > 60 ? d - 60 : 0;
                std::fprintf(out, "first diff at column %zu\n", d);
                std::fprintf(out, "A ...%s\n", la.c_str() + from);
                std::fprintf(out, "B ...%s\n", lb.c_str() + from);
                a.saveTo(outPath + ".a.sav");
                b.saveTo(outPath + ".b.sav");
            }
        } else if (detail == "roundtrip") {
            ok = false;
            detail = "save/load state mismatch";
        }
        std::fprintf(out, "%s savetest steps=%d file=%s %s\n", ok ? "OK" : "FAIL",
                     saveTestSteps, save::path().c_str(), detail.c_str());
        std::fclose(out);
        return ok ? 0 : 2;
    }

    // ---- invariant self-check ---------------------------------------------
    if (checkSteps > 0) {
        std::FILE* out = std::fopen(outPath.c_str(), "wb");
        if (!out) {
            std::fprintf(stderr, "cannot write %s\n", outPath.c_str());
            return 1;
        }
        std::string err;
        int steps = 0, hands = 0;
        // A deal that keeps buying a one-card table and discarding it back
        // never ends.  Remember recent states so the self-check reports the
        // loop instead of silently burning the whole step budget.
        std::deque<size_t> recent;
        for (; steps < checkSteps && !game.matchOver(); ++steps) {
            if (game.phase() == buraco::Phase::HandOver) {
                ++hands;
                std::fprintf(out, "# hand %d scored at step %d: p0 %d p1 %d\n", hands, steps,
                             game.player(0).lastHandPoints, game.player(1).lastHandPoints);
                game.continueAfterHand();
                recent.clear();
                continue;
            }
            game.autoStep();
            // Scoring hands the morto cards out of the accounting (endHand
            // clears them), so only check while a hand is actually in play.
            if (game.phase() == buraco::Phase::HandOver ||
                game.phase() == buraco::Phase::MatchOver)
                continue;
            if (!checkInvariants(game, err)) {
                std::fprintf(out, "FAIL at step %d (hand %d): %s\n%s", steps, hands + 1,
                             err.c_str(), stateLine(game).c_str());
                game.saveTo(outPath + ".sav");
                std::fclose(out);
                return 2;
            }
            const std::string st = stateLine(game);
            const size_t h = std::hash<std::string>{}(st);
            if (std::find(recent.begin(), recent.end(), h) != recent.end()) {
                std::fprintf(out, "FAIL at step %d (hand %d): stalled deal (state repeats)\n%s",
                             steps, hands + 1, st.c_str());
                game.saveTo(outPath + ".sav");
                std::fclose(out);
                return 2;
            }
            recent.push_back(h);
            if (recent.size() > 256) recent.pop_front();
        }
        std::fprintf(out, "OK steps=%d hands=%d p0=%d p1=%d\n", steps, hands,
                     game.player(0).total, game.player(1).total);
        std::fclose(out);
        return 0;
    }

    // ---- headless replay --------------------------------------------------
    if (headless) {
        std::FILE* out = std::fopen(outPath.c_str(), "wb");
        if (!out) {
            std::fprintf(stderr, "cannot write %s\n", outPath.c_str());
            return 1;
        }

        // Self-play: both seats driven by the same rule engine, used to check
        // deals, the morto, going out and the scoring without a window.
        if (autoSteps > 0) {
            resetSelection();
            int hands = 0;
            for (int i = 0; i < autoSteps;) {
                if (game.matchOver()) break;
                if (game.phase() == buraco::Phase::HandOver) {
                    std::fprintf(out, "# hand %d over: p0 %d p1 %d\n", hands + 1,
                                 game.player(0).lastHandPoints, game.player(1).lastHandPoints);
                    dumpState(game, out);
                    game.continueAfterHand();
                    ++hands;
                    continue;
                }
                game.autoStep();
                ++i;
            }
            ui::draw(video, game, ui);
            dumpScreen(video, out, "auto");
            dumpState(game, out);
            std::fclose(out);
            return 0;
        }

        std::FILE* in = std::fopen(scriptPath.c_str(), "rb");
        if (!in) {
            std::fclose(out);
            std::fprintf(stderr, "cannot open script %s\n", scriptPath.c_str());
            return 1;
        }

        resetSelection();
        dumpScreen(video, out, "start");
        char line[128];
        int step = 0;
        while (std::fgets(line, sizeof line, in)) {
            std::string tok(line);
            while (!tok.empty() && (tok.back() == '\n' || tok.back() == '\r' || tok.back() == ' '))
                tok.pop_back();
            while (!tok.empty() && tok.front() == ' ') tok.erase(tok.begin());
            if (tok.empty() || tok[0] == '#') continue;

            const SDL_Keycode k = keyFromToken(tok);
            if (!k) {
                std::fprintf(out, "# unknown token '%s'\n", tok.c_str());
                continue;
            }
            key(k);

            // Let the AI play until the turn comes back to the human.
            for (int guard = 0; guard < 40; ++guard) {
                using buraco::Phase;
                if (game.phase() == Phase::HandOver || game.phase() == Phase::MatchOver) break;
                if (game.current() == 0) break;
                game.update(0.75);
            }
            if (started && game.current() == 0 &&
                int(ui.selected.size()) != int(game.player(0).hand.size()))
                resetSelection();

            ui::draw(video, game, ui);
            char label[160];
            std::snprintf(label, sizeof label, "step %d: %s", ++step, tok.c_str());
            dumpScreen(video, out, label);
            dumpState(game, out);
        }
        std::fclose(in);
        std::fclose(out);
        return 0;
    }

    // ---- interactive window ----------------------------------------------
    dos::Renderer renderer;
    if (!renderer.init("Buraco", 2)) {
        std::fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (!guiAfterAuto) game.setMessage("Embaralhando. Por favor, corte... <ENTER>");
    resetSelection();

    uint64_t last = SDL_GetTicks64();
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            } else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                key(e.key.keysym.sym);
            }
        }

        const uint64_t now = SDL_GetTicks64();
        const double dt = double(now - last) / 1000.0;
        last = now;

        game.update(dt);
        if (started && game.current() == 0) {
            if (int(ui.selected.size()) != int(game.player(0).hand.size()))
                resetSelection();
        } else {
            ui.cursor = 0;
            if (!ui.selected.empty()) std::fill(ui.selected.begin(), ui.selected.end(), false);
        }

        ui::draw(video, game, ui);
        renderer.present(video, double(now) / 1000.0);
        SDL_Delay(8);
    }
    return 0;
}
