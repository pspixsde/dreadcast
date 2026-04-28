#pragma once

#include <raylib.h>

namespace dreadcast::ui {

struct CooldownVisual {
    float remaining{0.0F};
    float total{0.0F};
    float finishFlashSec{0.0F};
    float textSize{22.0F};
};

void drawCooldownOverlay(Rectangle rect, const CooldownVisual &visual, const Font &font);

/// Diagonal “reflecting” sheen only (no desaturation or cooldown readout). Used by cooldown
/// finish flash and by inventory stack-merged feedback. `durationSec` is the time for the sweep; keep
/// in sync with any caller that retires the timer on that schedule.
void drawItemStackSheenFlash(Rectangle rect, float finishFlashSec, float durationSec = 0.30F);

} // namespace dreadcast::ui
