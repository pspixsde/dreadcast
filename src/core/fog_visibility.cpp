#include "core/fog_visibility.hpp"

#include <algorithm>
#include <cmath>

#include "config.hpp"
#include "ecs/components.hpp"

namespace dreadcast {
namespace {

bool pointInRect(Vector2 p, Rectangle r) {
    return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
}

/// Ray vs axis-aligned rectangle: returns closest positive hit distance along `d` (|d|≈1), or
/// `maxT` if no hit. Rays starting inside the rect are treated as no hit (caller skips such walls).
float rayAabb2D(Vector2 o, Vector2 d, float maxT, Rectangle r) {
    if (pointInRect(o, r)) {
        return maxT;
    }
    float tMin = 0.0F;
    float tMax = maxT;
    const float EPS = 1.0e-6F;

    if (std::fabs(d.x) < EPS) {
        if (o.x < r.x || o.x > r.x + r.width) {
            return maxT;
        }
    } else {
        const float invDx = 1.0F / d.x;
        const float t0 = (r.x - o.x) * invDx;
        const float t1 = (r.x + r.width - o.x) * invDx;
        const float tNear = std::min(t0, t1);
        const float tFar = std::max(t0, t1);
        tMin = std::max(tMin, tNear);
        tMax = std::min(tMax, tFar);
    }
    if (tMin > tMax) {
        return maxT;
    }

    if (std::fabs(d.y) < EPS) {
        if (o.y < r.y || o.y > r.y + r.height) {
            return maxT;
        }
    } else {
        const float invDy = 1.0F / d.y;
        const float t0 = (r.y - o.y) * invDy;
        const float t1 = (r.y + r.height - o.y) * invDy;
        const float tNear = std::min(t0, t1);
        const float tFar = std::max(t0, t1);
        tMin = std::max(tMin, tNear);
        tMax = std::min(tMax, tFar);
    }
    if (tMin > tMax) {
        return maxT;
    }
    if (tMax < 0.0F) {
        return maxT;
    }
    const float tHit = tMin >= 0.0F ? tMin : tMax;
    if (tHit < 0.001F || tHit > maxT) {
        return maxT;
    }
    return tHit;
}

} // namespace

void buildVisibilityPolygonWorld(Vector2 playerWorld, entt::registry &registry, float maxRadius,
                                 std::vector<Vector2> &outWorldBoundary) {
    const int N = config::FOG_VISIBILITY_SAMPLES;
    outWorldBoundary.clear();
    outWorldBoundary.reserve(static_cast<size_t>(N));

    const float pi2 = PI * 2.0F;
    const auto walls = registry.view<ecs::Wall, ecs::Transform>();

    for (int i = 0; i < N; ++i) {
        const float a = (static_cast<float>(i) / static_cast<float>(N)) * pi2;
        Vector2 dir{std::cos(a), std::sin(a)};
        const float ilen = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (ilen > 1.0e-6F) {
            dir.x /= ilen;
            dir.y /= ilen;
        }

        float dist = maxRadius;

        for (const auto w : walls) {
            const auto &t = registry.get<ecs::Transform>(w);
            const auto &wall = registry.get<ecs::Wall>(w);
            const Rectangle rect{t.position.x - wall.halfW, t.position.y - wall.halfH,
                                 wall.halfW * 2.0F, wall.halfH * 2.0F};

            const float dx = t.position.x - playerWorld.x;
            const float dy = t.position.y - playerWorld.y;
            const float pad = maxRadius + std::max(wall.halfW, wall.halfH) * 2.0F + 8.0F;
            if (dx * dx + dy * dy > pad * pad) {
                continue;
            }

            const float tHit = rayAabb2D(playerWorld, dir, maxRadius, rect);
            if (tHit < dist) {
                dist = tHit;
            }
        }

        outWorldBoundary.push_back(
            Vector2{playerWorld.x + dir.x * dist, playerWorld.y + dir.y * dist});
    }
}

} // namespace dreadcast
