#include "ui/cooldown_overlay.hpp"

#include <algorithm>
#include <cstdio>
#include <cmath>

namespace dreadcast::ui {

namespace {
constexpr float kDefaultFinishFlashSec = 0.30F;
} // namespace

void drawItemStackSheenFlash(Rectangle rect, float finishFlashSec, float durationSec) {
    if (finishFlashSec <= 0.001F) {
        return;
    }
    const float t = std::clamp(finishFlashSec / durationSec, 0.0F, 1.0F);

    const Vector2 p0{rect.x, rect.y + rect.height};
    const Vector2 p1{rect.x + rect.width, rect.y};
    const float diagLen = std::sqrt((p1.x - p0.x) * (p1.x - p0.x) + (p1.y - p0.y) * (p1.y - p0.y));
    if (diagLen <= 0.001F) {
        return;
    }
    const Vector2 u{(p1.x - p0.x) / diagLen, (p1.y - p0.y) / diagLen};
    const Vector2 v{-u.y, u.x};
    const Vector2 center{p0.x + t * (p1.x - p0.x), p0.y + t * (p1.y - p0.y)};

    const float halfAlongU = 0.045F * std::min(rect.width, rect.height);
    const float halfAcrossV = 0.72F * diagLen;

    const auto offset = [](Vector2 a, float su, const Vector2 &dirU, float sv, const Vector2 &dirV) {
        return Vector2{a.x + su * dirU.x + sv * dirV.x, a.y + su * dirU.y + sv * dirV.y};
    };
    const Vector2 c1 = offset(center, -halfAlongU, u, -halfAcrossV, v);
    const Vector2 c2 = offset(center, -halfAlongU, u, +halfAcrossV, v);
    const Vector2 c3 = offset(center, +halfAlongU, u, +halfAcrossV, v);
    const Vector2 c4 = offset(center, +halfAlongU, u, -halfAcrossV, v);

    // Match cooldown sheen: warm highlight; alpha falls off as t→1. Tweak: duration passed in, color,
    // halfAlongU / halfAcrossV in this file for visual parity with drawCooldownOverlay’s flash.
    const Color flash = Fade(Color{255, 245, 200, 255}, 0.85F * (1.0F - t));

    BeginScissorMode(static_cast<int>(rect.x), static_cast<int>(rect.y),
                     static_cast<int>(rect.width), static_cast<int>(rect.height));
    DrawTriangle(c1, c2, c3, flash);
    DrawTriangle(c1, c3, c4, flash);
    EndScissorMode();
}

void drawCooldownOverlay(Rectangle rect, const CooldownVisual &visual, const Font &font) {
    const bool hasCooldown = visual.remaining > 0.001F && visual.total > 0.001F;
    const bool hasFlash = visual.finishFlashSec > 0.001F;
    if (!hasCooldown && !hasFlash) {
        return;
    }

    if (hasCooldown) {
        DrawRectangleRec(rect, Fade(Color{160, 160, 160, 255}, 0.18F));

        const float ratio = std::clamp(visual.remaining / visual.total, 0.0F, 1.0F);
        const Vector2 center{rect.x + rect.width * 0.5F, rect.y + rect.height * 0.5F};
        const float radius =
            std::sqrt(rect.width * rect.width + rect.height * rect.height) * 0.5F + 2.0F;
        const float maskStartDeg = -90.0F + (1.0F - ratio) * 360.0F;
        const float maskEndDeg = maskStartDeg + ratio * 360.0F;

        BeginScissorMode(static_cast<int>(rect.x), static_cast<int>(rect.y),
                         static_cast<int>(rect.width), static_cast<int>(rect.height));
        DrawCircleSector(center, radius, maskStartDeg, maskEndDeg, 72, Fade(Color{20, 20, 22, 255}, 0.55F));
        EndScissorMode();

        char cdBuf[16];
        std::snprintf(cdBuf, sizeof(cdBuf), "%d",
                      static_cast<int>(std::ceil(static_cast<double>(visual.remaining))));
        const Vector2 cdDim = MeasureTextEx(font, cdBuf, visual.textSize, 1.0F);
        const float tx = rect.x + (rect.width - cdDim.x) * 0.5F;
        const float ty = rect.y + (rect.height - cdDim.y) * 0.5F;
        DrawTextEx(font, cdBuf, {tx + 1.0F, ty + 1.0F}, visual.textSize, 1.0F, Fade(BLACK, 180));
        DrawTextEx(font, cdBuf, {tx, ty}, visual.textSize, 1.0F, Color{200, 220, 255, 255});
    }

    if (hasFlash) {
        // Same sheen as `drawItemStackSheenFlash`; `finishFlashSec` in GameplayScene counts up
        // during ~0.30s. See `kDefaultFinishFlashSec` in cooldown_overlay.cpp.
        drawItemStackSheenFlash(rect, visual.finishFlashSec, kDefaultFinishFlashSec);
    }
}

} // namespace dreadcast::ui
