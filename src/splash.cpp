// Hymnal - the splash: a song's waveform drawn large in solid bars, bleeding off the right edge, the bars round the
// middle in pink (the song playing), the name, the version, the credits and what's loading. Atelier greys and the
// Cursed pink; drawn with ImGui's draw list so it's sharp at any size.
#include "splash.h"

#include <imgui.h>

#include <cfloat>
#include <cmath>

#ifndef HYMNAL_VERSION
#define HYMNAL_VERSION "1.0.0"
#endif

namespace Splash {

static ImFont *titleFont = nullptr, *smallFont = nullptr;

void setFonts(ImFont* title, ImFont* small) { titleFont = title; smallFont = small; }

static const ImU32 BG = IM_COL32(0x1a, 0x1a, 0x1a, 255), LINE = IM_COL32(0x33, 0x33, 0x33, 255),
                   TEXT = IM_COL32(0xd6, 0xd6, 0xd6, 255), DIM = IM_COL32(0x9a, 0x9a, 0x9a, 255),
                   PINK = IM_COL32(0x00, 0x7b, 0xb7, 255), DEEP = IM_COL32(0x00, 0x40, 0x67, 255),
                   DARK = IM_COL32(0x3a, 0x3a, 0x3a, 255), MID = IM_COL32(0x55, 0x55, 0x55, 255);

static void text(ImDrawList* dl, ImFont* f, float size, ImVec2 p, ImU32 col, const char* s) {
    if (!f) f = ImGui::GetFont();
    dl->AddText(f, size, p, col, s);
}
static float textWidth(ImFont* f, float size, const char* s) {
    if (!f) f = ImGui::GetFont();
    return f->CalcTextSizeA(size, FLT_MAX, 0, s).x;
}

void draw(ImDrawList* dl, float x, float y, const char* status, float progress) {
    const ImVec2 o(x, y), end(x + W, y + H);
    dl->PushClipRect(o, end, true);
    dl->AddRectFilled(o, end, BG);

    // the art: a song's waveform, rounded bars mirrored about a centre line, swelling into a chorus
    const float cy = y + 180, x0 = x + 330, barW = 9, gap = 5;
    for (int i = 0; i < 40; ++i) {
        const float bx = x0 + i * (barW + gap);
        if (bx > end.x) break;
        const float t = i / 39.0f;
        const float env = 0.35f + 0.65f * std::sin(3.14159f * std::fmin(1.0f, t * 1.15f));
        const float wob = 0.55f + 0.45f * std::fabs(std::sin(i * 1.7f) * std::cos(i * 0.6f));
        const float h = 120 * env * wob + 8;
        const ImU32 col = (i >= 12 && i <= 22) ? PINK : (i >= 9 && i <= 25) ? DEEP : (i % 3 == 0 ? MID : DARK);
        dl->AddRectFilled(ImVec2(bx, cy - h), ImVec2(bx + barW, cy + h), col, barW / 2);
    }

    // the name: "Hym" in the text colour, "nal" in pink
    const float ts = 50, tx = x + 40, ty = y + 92;
    text(dl, titleFont, ts, ImVec2(tx, ty), TEXT, "Hym");
    text(dl, titleFont, ts, ImVec2(tx + textWidth(titleFont, ts, "Hym"), ty), PINK, "nal");
    text(dl, nullptr, 16, ImVec2(tx + 2, ty + 62), DIM, "Version " HYMNAL_VERSION);
    text(dl, nullptr, 16, ImVec2(tx + 2, ty + 84), DIM, "Songs by Cursed Entertainment");

    const float cs = 13, cry = y + 306;
    text(dl, smallFont, cs, ImVec2(tx, cry), DIM, "ACE-Step 1.5 (ACE Studio and StepFun, Apache-2.0), run by acestep.cpp (MIT).");
    text(dl, smallFont, cs, ImVec2(tx, cry + 17), DIM, "Built with ggml, Dear ImGui and SDL2.");
    text(dl, smallFont, cs, ImVec2(tx, cry + 34), DIM, "Farica\xe2\x98\x86Kimora \xc2\xa9 Cursed Entertainment");

    if (status) text(dl, nullptr, 16, ImVec2(tx, y + 368), TEXT, status);
    if (progress >= 0) {
        const float p = progress > 1 ? 1 : progress;
        dl->AddRectFilled(ImVec2(x, end.y - 3), end, LINE);
        dl->AddRectFilled(ImVec2(x, end.y - 3), ImVec2(x + W * p, end.y), PINK);
    }
    dl->AddRect(o, end, LINE);
    dl->PopClipRect();
}

}  // namespace Splash
