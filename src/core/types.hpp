#pragma once

#include <cmath>

#include <raylib.h>

namespace dreadcast {

using Vec2 = Vector2;

inline float Vec2Length(Vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }

inline Vec2 Vec2Normalize(Vec2 v) {
    const float len = Vec2Length(v);
    if (len <= 0.0F) {
        return {0.0F, 0.0F};
    }
    return {v.x / len, v.y / len};
}

inline float Vec2Dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }

inline Vec2 Vec2Lerp(Vec2 a, Vec2 b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

inline float RadToDeg(float rad) { return rad * 180.0F / 3.14159265358979323846F; }

/// Angle in radians from +X toward +Y (matches atan2f(y, x) for screen coords if Y grows down).
inline float AngleToward(Vec2 from, Vec2 to) {
    return std::atan2f(to.y - from.y, to.x - from.x);
}

} // namespace dreadcast
