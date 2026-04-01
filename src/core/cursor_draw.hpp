#pragma once

#include <raylib.h>

namespace dreadcast {

class ResourceManager;

enum class CursorKind { Default, Aim, Interact, Invalid };

/// Draws a 128x128 cursor texture with the correct hotspot (screen space).
void drawCustomCursor(ResourceManager &resources, CursorKind kind, Vector2 screenMouse);

} // namespace dreadcast
