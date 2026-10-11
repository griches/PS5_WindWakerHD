/*
 * Wind Waker HD for the PlayStation 5 - the DualSense's symbols on the game's button prompts.
 *
 * The game has no pictures of its buttons to replace. A prompt is a blank button (a texture) with
 * its letter printed on it as text, in the HUD font (CKingMain), the font of every label beside it
 * ("Crouch", "Back", the rupee count): repainting the font's A would repaint every A.
 *
 * So two things are done, and neither stores anything of the game:
 *
 *   1. Text. The game prints all 2D text through one function (nw::font's PrintImpl, hooked by
 *      runtime/src/rtl_text_hooks.cpp). A text that is exactly one button's name is printed as
 *      one character the English game never prints (the font carries kana): A, B, X and Y as
 *      the one that stands for the DualSense button that plays that Wii U button (Controls > Face
 *      buttons decides: by default, by position, Circle is A, Cross B, Triangle X and Square Y),
 *      and R, ZR, L and ZL as the ones for R1, R2, L1 and L2.
 *   2. The font. When the renderer uploads that font's sheet (known by the hash of its bytes),
 *      those characters' cells are painted over: a cross, a circle, a square and a triangle drawn
 *      here in the font's own two tones (white, and the grey its outline is), and the shoulder
 *      names put together from the sheet's own letters and digits, narrowed to fit one cell.
 *
 * The cells' places are the USA game's CKingMain.bffnt (cell 62x74, 16 a row, 13 rows a sheet,
 * stored bottom row first). A font this file does not know is left alone, and with
 * WWHD_PS5_GLYPHS=0 so is the text.
 *
 * SPDX-License-Identifier: MIT
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <sys/stat.h>

#include "input_map.h"
#include "runtime.h"

namespace {
bool glyphs_enabled() {
    static const bool on = [] { const char* e = getenv("WWHD_PS5_GLYPHS"); return !e || strcmp(e, "0") != 0; }();
    return on;
}
// set once the font's sheet has been painted: until then the text is left as it is
bool g_painted = false;

enum Symbol { kCross, kCircle, kSquare, kTriangle };
// the character that stands for each symbol, its cell in sheet 0 (row, column) and where the
// font puts it: `left` is the glyph's offset from the pen, `advance` the pen's step
struct Slot { uint16_t code; int row, col, left, advance; };
const Slot kSlots[4] = {
    {0x3042, 7, 10, -2, 50},  // cross
    {0x3044, 7, 12, 0, 50},   // circle
    {0x3046, 7, 14, 3, 50},   // square
    {0x3048, 8, 0, -2, 50},   // triangle
};
constexpr uint64_t kMainFontHash = 0xaf39e392b7598422ull;
constexpr int kCellW = 62, kCellH = 74, kPerRow = 16, kSheet = 1024, kBaseline = 58;

float segment(float px, float py, float ax, float ay, float bx, float by) {
    const float vx = bx - ax, vy = by - ay, wx = px - ax, wy = py - ay;
    const float t = std::clamp((wx * vx + wy * vy) / (vx * vx + vy * vy), 0.0f, 1.0f);
    return std::hypot(wx - t * vx, wy - t * vy);
}
// distance from the symbol's stroke, the symbol centred on the origin, y down
float distance(Symbol s, float x, float y) {
    const float r = 15.5f;
    switch (s) {
    case kCross: return std::min(segment(x, y, -r, -r, r, r), segment(x, y, -r, r, r, -r));
    case kCircle: return std::fabs(std::hypot(x, y) - (r + 1.5f));
    case kSquare: {
        const float qx = std::fabs(x) - r, qy = std::fabs(y) - r;
        const float outside = std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f);
        return std::fabs(outside);
    }
    default: {
        const float top = -r - 2, bottom = r - 1, half = r + 3;
        return std::min({segment(x, y, 0, top, half, bottom), segment(x, y, half, bottom, -half, bottom), segment(x, y, -half, bottom, 0, top)});
    }
    }
}
// one cell, as the font's sheet holds a glyph: 255 its body, 128 its outline, 0 nothing
void draw_cell(Symbol s, const Slot& slot, uint8_t* cell) {
    const float cx = slot.advance * 0.5f - slot.left, cy = kBaseline - 22.0f;
    const float stroke = 3.4f, outline = 3.6f;
    for (int y = 0; y < kCellH; y++)
        for (int x = 0; x < kCellW; x++) {
            const float d = distance(s, x + 0.5f - cx, y + 0.5f - cy);
            const float body = std::clamp(stroke - d + 0.5f, 0.0f, 1.0f), edge = std::clamp(stroke + outline - d + 0.5f, 0.0f, 1.0f);
            cell[y * kCellW + x] = (uint8_t)std::lround(128.0f * edge + 127.0f * body);
        }
}

// ---- BC4 (one channel, 4x4 blocks of 8 bytes): the font's sheets
void bc4_decode(const uint8_t* b, uint8_t out[16]) {
    uint8_t p[8] = {b[0], b[1]};
    if (b[0] > b[1]) for (int i = 1; i < 7; i++) p[i + 1] = (uint8_t)(((7 - i) * b[0] + i * b[1]) / 7);
    else { for (int i = 1; i < 5; i++) p[i + 1] = (uint8_t)(((5 - i) * b[0] + i * b[1]) / 5); p[6] = 0; p[7] = 255; }
    uint64_t bits = 0;
    for (int i = 0; i < 6; i++) bits |= (uint64_t)b[2 + i] << (8 * i);
    for (int i = 0; i < 16; i++) out[i] = p[(bits >> (3 * i)) & 7];
}
void bc4_encode(const uint8_t in[16], uint8_t* b) {
    uint8_t lo = 255, hi = 0;
    for (int i = 0; i < 16; i++) { lo = std::min(lo, in[i]); hi = std::max(hi, in[i]); }
    if (lo == hi) { b[0] = hi; b[1] = lo; memset(b + 2, 0, 6); return; }
    uint8_t p[8] = {hi, lo};
    for (int i = 1; i < 7; i++) p[i + 1] = (uint8_t)(((7 - i) * hi + i * lo) / 7);
    uint64_t bits = 0;
    for (int i = 0; i < 16; i++) {
        int best = 0, error = 256;
        for (int k = 0; k < 8; k++) { const int e = std::abs(int(p[k]) - int(in[i])); if (e < error) { error = e; best = k; } }
        bits |= (uint64_t)best << (3 * i);
    }
    b[0] = hi; b[1] = lo;
    for (int i = 0; i < 6; i++) b[2 + i] = (uint8_t)(bits >> (8 * i));
}
// a cell of sheet 0, the right way up (the sheet stores its bottom row first, so cells are upside down in it)
void cell_place(int row, int col, int& x0, int& yTop) { x0 = col * (kCellW + 1) + 1; yTop = kSheet - (row + 1) * (kCellH + 1); }
void read_cell(const std::vector<uint8_t>& data, int row, int col, uint8_t* cell) {
    int x0, yTop;
    cell_place(row, col, x0, yTop);
    for (int by = yTop / 4; by <= (yTop + kCellH - 1) / 4; by++)
        for (int bx = x0 / 4; bx <= (x0 + kCellW - 1) / 4; bx++) {
            uint8_t px[16];
            bc4_decode(data.data() + (size_t(by) * (kSheet / 4) + bx) * 8, px);
            for (int i = 0; i < 16; i++) {
                const int x = bx * 4 + (i & 3) - x0, y = by * 4 + (i >> 2) - yTop;
                if (x >= 0 && x < kCellW && y >= 0 && y < kCellH) cell[(kCellH - 1 - y) * kCellW + x] = px[i];
            }
        }
}
void write_cell(std::vector<uint8_t>& data, int row, int col, const uint8_t* cell) {
    int x0, yTop;
    cell_place(row, col, x0, yTop);
    for (int by = yTop / 4; by <= (yTop + kCellH - 1) / 4; by++)
        for (int bx = x0 / 4; bx <= (x0 + kCellW - 1) / 4; bx++) {
            uint8_t* block = data.data() + (size_t(by) * (kSheet / 4) + bx) * 8;
            uint8_t px[16];
            bc4_decode(block, px);
            for (int i = 0; i < 16; i++) {
                const int x = bx * 4 + (i & 3) - x0, y = by * 4 + (i >> 2) - yTop;
                if (x >= 0 && x < kCellW && y >= 0 && y < kCellH) px[i] = cell[(kCellH - 1 - y) * kCellW + x];
            }
            bc4_encode(px, block);
        }
}
// The shoulder buttons' names, each as one glyph: the game's own letter and digit from this sheet,
// narrowed to share a cell ("R1" as two characters is wider than the R button's text box, and wraps).
// A source glyph: its cell and how wide the font says it is.
struct Source { int row, col, width; };
const Source kR{3, 2, 41}, kL{2, 12, 36}, kOne{1, 1, 33}, kTwo{1, 2, 44};
const Slot kShoulder[4] = {{0x304A, 8, 2, -2, 50}, {0x304B, 8, 3, -2, 50}, {0x304C, 8, 4, -2, 50}, {0x304D, 8, 5, -1, 50}};  // R1 R2 L1 L2
void compose(std::vector<uint8_t>& data, const Slot& slot, const Source& letter, const Source& digit) {
    uint8_t a[kCellW * kCellH] = {}, b[kCellW * kCellH] = {}, out[kCellW * kCellH] = {};
    read_cell(data, letter.row, letter.col, a);
    read_cell(data, digit.row, digit.col, b);
    const float scale = 0.6f;
    const float total = (letter.width + digit.width) * scale, start = slot.advance * 0.5f - slot.left - total * 0.5f;
    const auto place = [&](const uint8_t* src, const Source& from, float at) {
        for (int y = 0; y < kCellH; y++)
            for (int x = 0; x < kCellW; x++) {
                const float sx = (x + 0.5f - at) / scale - 0.5f;  // in the source glyph
                if (sx < 0 || sx > from.width - 1) continue;
                const int x0 = (int)sx;
                const float f = sx - x0;
                const float v = src[y * kCellW + x0] * (1 - f) + src[y * kCellW + std::min(x0 + 1, kCellW - 1)] * f;
                out[y * kCellW + x] = std::max(out[y * kCellW + x], (uint8_t)std::lround(v));
            }
    };
    place(a, letter, start);
    place(b, digit, start + letter.width * scale);
    write_cell(data, slot.row, slot.col, out);
}
void paint_main_font(std::vector<uint8_t>& data, uint32_t w, uint32_t h) {
    // (the shoulder names first: they read cells of the sheet as the game made it)
    compose(data, kShoulder[0], kR, kOne);
    compose(data, kShoulder[1], kR, kTwo);
    compose(data, kShoulder[2], kL, kOne);
    compose(data, kShoulder[3], kL, kTwo);
    for (int s = 0; s < 4; s++) {
        uint8_t cell[kCellW * kCellH];
        draw_cell((Symbol)s, kSlots[s], cell);
        write_cell(data, kSlots[s].row, kSlots[s].col, cell);
    }
    (void)w; (void)h;
}
const char* blank_list() { static const char* e = getenv("WWHD_PS5_BLANK"); return e && *e ? e : nullptr; }
}  // namespace

namespace gfxvk {
// Every level of every texture the game uploads comes through here, as the bytes the GPU gets
// (gfx/vulkan/surfaces.cpp, the render thread).
void ps5_texture(uint64_t hash, uint32_t level, uint32_t vkFormat, bool compressed, uint32_t guestFormat,
                 std::vector<uint8_t>& data, uint32_t w, uint32_t h, uint32_t slices) {
    if (glyphs_enabled() && hash == kMainFontHash && level == 0 && w == kSheet && h == kSheet &&
        data.size() >= size_t(kSheet / 4) * (kSheet / 4) * 8) {
        paint_main_font(data, w, h);
        if (!g_painted) fprintf(stderr, "[glyphs] the HUD font's sheet has the DualSense's symbols\n");
        g_painted = true;
    }
    // debug (WWHD_PS5_DUMP_TEXTURES=1): level 0 of each texture up to 1024 texels wide, once, to
    // /app0/userdata/texdump/<hash>_<w>x<h>_<vkFormat>_<guestFormat>.bin (the first slice)
    static const bool dump = [] {
        const char* e = getenv("WWHD_PS5_DUMP_TEXTURES");
        if (!e || !*e || !strcmp(e, "0")) return false;
        mkdir("/app0/userdata/texdump", 0777);
        return true;
    }();
    if (dump && level == 0 && w <= 1024 && h <= 1024 && !data.empty()) {
        static int written = 0;
        char path[160];
        snprintf(path, sizeof path, "/app0/userdata/texdump/%016llx_%ux%u_%u_%x.bin", (unsigned long long)hash, w, h, vkFormat, guestFormat);
        struct stat st;
        if (written < 6000 && stat(path, &st) != 0) {
            if (FILE* f = fopen(path, "wb")) {
                fwrite(data.data(), 1, data.size() / (slices ? slices : 1), f);
                fclose(f);
                ++written;
            }
        }
    }
    // debug (WWHD_PS5_BLANK=<hash prefix>): zero a texture, to see what is drawn from it
    if (const char* blank = blank_list()) {
        char name[24];
        snprintf(name, sizeof name, "%016llx", (unsigned long long)hash);
        if (level == 0 && (slices > 1 || w > 1024 || h > 1024))
            fprintf(stderr, "[glyphs] texture %s %ux%u x%u vk %u gx2 %x\n", name, w, h, slices, vkFormat, guestFormat);
        if (!strncmp(name, blank, strlen(blank))) memset(data.data(), 0, data.size());
    }
    (void)compressed;
}
}  // namespace gfxvk

// nw::font's PrintImpl is about to print `length` UTF-16 units at guest address `text` (the game's
// thread): if they are exactly a button's name, the text to print in its place (and its length).
uint32_t ps5_button_text(uint32_t text, int32_t& length) {
    if (!glyphs_enabled() || !g_painted || length < 1 || length > 2) return text;
    const uint16_t a = ld16(text), b = length == 2 ? ld16(text + 2) : 0;
    // the eight texts, written once into guest memory the game never sees otherwise
    static const uint32_t table = [] {
        const uint32_t at = mem::host_alloc(8 * 8, 4);
        const uint16_t texts[8][2] = {{kSlots[0].code, 0}, {kSlots[1].code, 0}, {kSlots[2].code, 0}, {kSlots[3].code, 0},
                                      {kShoulder[0].code, 0}, {kShoulder[1].code, 0}, {kShoulder[2].code, 0}, {kShoulder[3].code, 0}};
        for (int i = 0; i < 8; i++) { st16(at + 8 * i, texts[i][0]); st16(at + 8 * i + 2, texts[i][1]); st16(at + 8 * i + 4, 0); }
        return at;
    }();
    int entry = -1;
    if (length == 1) {
        using namespace input_map;
        int action = -1;
        switch (a) { case 'A': action = kA; break; case 'B': action = kB; break; case 'X': action = kX; break; case 'Y': action = kY; break;
                     case 'R': entry = 4; break; case 'L': entry = 6; break; default: break; }
        if (action >= 0) {
            // the DualSense button that plays it: south Cross, east Circle, west Square, north Triangle
            switch (current().pad[action]) { case kPadA: entry = kCross; break; case kPadB: entry = kCircle; break;
                                             case kPadX: entry = kSquare; break; case kPadY: entry = kTriangle; break; default: break; }
        }
    } else if (a == 'Z' && b == 'R') entry = 5;
    else if (a == 'Z' && b == 'L') entry = 7;
    if (entry < 0) return text;
    length = 1;
    return table + 8 * entry;
}
