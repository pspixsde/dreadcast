#include "core/cursor_draw.hpp"

#include "config.hpp"
#include "core/resource_manager.hpp"

namespace dreadcast {

namespace {

struct CursorDef {
    const char *path;
    Vector2 hotspot;
};

const CursorDef kCursors[] = {
    {"assets/cursors/default.png", {33.0F, 4.0F}},
    {"assets/cursors/aim.png", {63.0F, 64.0F}},
    {"assets/cursors/interact.png", {61.0F, 42.0F}},
    {"assets/cursors/invalid.png", {63.0F, 64.0F}},
};

} // namespace

void drawCustomCursor(ResourceManager &resources, CursorKind kind, Vector2 screenMouse) {
    const int i = static_cast<int>(kind);
    if (i < 0 || i >= 4) {
        return;
    }
    const CursorDef &def = kCursors[i];
    const Texture2D tex = resources.getTexture(def.path);
    if (tex.id == 0) {
        return;
    }
    const Rectangle src{0.0F, 0.0F, static_cast<float>(tex.width),
                          static_cast<float>(tex.height)};
    const float s = config::CURSOR_DISPLAY_SCALE;
    const float dw = static_cast<float>(tex.width) * s;
    const float dh = static_cast<float>(tex.height) * s;
    const float hx = def.hotspot.x * s;
    const float hy = def.hotspot.y * s;
    const Rectangle dst{screenMouse.x - hx, screenMouse.y - hy, dw, dh};
    DrawTexturePro(tex, src, dst, {0.0F, 0.0F}, 0.0F, WHITE);
}

} // namespace dreadcast
