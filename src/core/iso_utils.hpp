#pragma once

#include <cmath>

#include <raylib.h>

#include "core/types.hpp"
#include "ecs/components.hpp"

namespace dreadcast {

/// Standard 2:1 isometric projection (world XY to screen XY in camera space).
inline Vec2 worldToIso(Vec2 w) { return {w.x - w.y, (w.x + w.y) * 0.5F}; }

inline Vec2 isoToWorld(Vec2 iso) {
    return {(iso.x + 2.0F * iso.y) * 0.5F, (2.0F * iso.y - iso.x) * 0.5F};
}

/// Snap atan2(dy, dx) to 8 directions (E, NE, N, NW, W, SW, S, SE).
inline ecs::FacingDir facingFromAngle(float radians) {
    float d = radians * RAD2DEG;
    while (d < 0.0F) {
        d += 360.0F;
    }
    while (d >= 360.0F) {
        d -= 360.0F;
    }
    const int seg = static_cast<int>((d + 22.5F) / 45.0F) % 8;
    return static_cast<ecs::FacingDir>(seg);
}

} // namespace dreadcast
