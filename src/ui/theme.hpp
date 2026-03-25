#pragma once

#include <raylib.h>

namespace dreadcast::ui::theme {

inline constexpr Color BG_DARK = {20, 12, 12, 255};
inline constexpr Color PANEL_FILL = {35, 18, 18, 240};
inline constexpr Color PANEL_BORDER = {180, 50, 30, 255};
inline constexpr Color BTN_FILL = {50, 22, 18, 255};
inline constexpr Color BTN_HOVER = {75, 32, 22, 255};
inline constexpr Color BTN_BORDER = {180, 60, 30, 255};
inline constexpr Color BTN_DISABLED_FILL = {30, 18, 18, 255};
inline constexpr Color BTN_DISABLED_HOVER = {38, 22, 22, 255};
inline constexpr Color BTN_DISABLED_BORDER = {70, 40, 35, 255};
inline constexpr Color SLOT_FILL = {40, 20, 18, 255};
inline constexpr Color SLOT_BORDER = {130, 50, 30, 255};
inline constexpr Color PORTRAIT_FILL = {45, 22, 20, 255};
inline constexpr Color PORTRAIT_RING = {180, 60, 30, 255};
inline constexpr Color LABEL_TEXT = {220, 160, 120, 255};
inline constexpr Color SUBTITLE_TEXT = {200, 150, 120, 255};
inline constexpr Color MUTED_TEXT = {175, 130, 100, 255};
inline constexpr Color CLEAR_BG = {18, 10, 10, 255};
inline constexpr Color MENU_BG = {18, 10, 10, 255};
inline constexpr Color HUD_BACKING = {0, 0, 0, 140};
inline constexpr Color INVENTORY_OVERLAY = {0, 0, 0, 160};
inline constexpr Color PAUSE_OVERLAY = {0, 0, 0, 180};

} // namespace dreadcast::ui::theme
