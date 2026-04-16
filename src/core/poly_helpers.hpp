#pragma once

#include <cmath>
#include <vector>

#include <raylib.h>

namespace dreadcast {

/// Point-in-polygon (winding / ray-cast) for closed `poly` with at least 3 vertices.
inline bool pointInPolygon(Vector2 p, const std::vector<Vector2> &poly) {
    const int n = static_cast<int>(poly.size());
    if (n < 3) {
        return false;
    }
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        const Vector2 &vi = poly[static_cast<size_t>(i)];
        const Vector2 &vj = poly[static_cast<size_t>(j)];
        const float dy = vj.y - vi.y;
        if (std::fabs(dy) < 1.0e-8F) {
            continue;
        }
        if ((vi.y > p.y) == (vj.y > p.y)) {
            continue;
        }
        const float t = (p.y - vi.y) / dy;
        const float xInt = vi.x + t * (vj.x - vi.x);
        if (p.x < xInt) {
            inside = !inside;
        }
    }
    return inside;
}

/// True if open segment (a,b) intersects any edge of closed polygon `poly` (>=3 verts).
inline bool segmentIntersectsPolygonEdges(Vector2 a, Vector2 b,
                                          const std::vector<Vector2> &poly) {
    const int n = static_cast<int>(poly.size());
    if (n < 3) {
        return false;
    }
    for (int i = 0; i < n; ++i) {
        const Vector2 p0 = poly[static_cast<size_t>(i)];
        const Vector2 p1 = poly[static_cast<size_t>((i + 1) % n)];
        Vector2 hit{};
        if (CheckCollisionLines(a, b, p0, p1, &hit)) {
            (void)hit;
            return true;
        }
    }
    return false;
}

/// Closest positive ray distance along unit `dir` from `origin` to polygon edge, or `maxT`.
inline float rayDistanceToPolygonEdges(Vector2 origin, Vector2 dir, float maxT,
                                       const std::vector<Vector2> &poly) {
    const int n = static_cast<int>(poly.size());
    if (n < 3) {
        return maxT;
    }
    float best = maxT;
    for (int i = 0; i < n; ++i) {
        const Vector2 p0 = poly[static_cast<size_t>(i)];
        const Vector2 p1 = poly[static_cast<size_t>((i + 1) % n)];
        const Vector2 e{p1.x - p0.x, p1.y - p0.y};
        const float cross = dir.x * e.y - dir.y * e.x;
        if (std::fabs(cross) < 1.0e-8F) {
            continue;
        }
        const Vector2 r{p0.x - origin.x, p0.y - origin.y};
        const float t = (r.x * e.y - r.y * e.x) / cross;
        const float u = (r.x * dir.y - r.y * dir.x) / cross;
        if (t >= 0.0F && t < best && u >= 0.0F && u <= 1.0F) {
            best = t;
        }
    }
    return best;
}

} // namespace dreadcast
