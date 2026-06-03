#pragma once

#include <raylib.h>

namespace dreadcast {
struct ItemData;
class ResourceManager;
} // namespace dreadcast

namespace dreadcast::ui {

/// Shared slot drawing for inventory panel, HUD strips, anvil, and editor previews.
struct SlotWidget {
    static void drawSurface(Rectangle slotRect, const dreadcast::ItemData *item, bool ghost);
    static void drawIcon(const dreadcast::ItemData &item, dreadcast::ResourceManager &resources,
                          Rectangle slotRect, Color tint = WHITE);
};

} // namespace dreadcast::ui
