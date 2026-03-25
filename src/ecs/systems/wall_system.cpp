#include "ecs/systems/wall_system.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include <entt/entt.hpp>
#include <raylib.h>

#include "config.hpp"
#include "ecs/components.hpp"

namespace dreadcast::ecs {

namespace {

Rectangle wallRect(const Transform &t, const Wall &w) {
    return {t.position.x - w.halfW, t.position.y - w.halfH, w.halfW * 2.0F, w.halfH * 2.0F};
}

Rectangle entityRect(const Vector2 &pos, const Sprite &s) {
    return {pos.x - s.width * 0.5F, pos.y - s.height * 0.5F, s.width, s.height};
}

bool circleRectOverlap(Vector2 center, float r, Rectangle rect) {
    const float cx = std::clamp(center.x, rect.x, rect.x + rect.width);
    const float cy = std::clamp(center.y, rect.y, rect.y + rect.height);
    const float dx = center.x - cx;
    const float dy = center.y - cy;
    return dx * dx + dy * dy <= r * r;
}

void separateFromWall(Vector2 &pos, const Sprite &spr, const Rectangle &wallR) {
    Rectangle er = entityRect(pos, spr);
    if (!CheckCollisionRecs(er, wallR)) {
        return;
    }

    const float overlapLeft = (er.x + er.width) - wallR.x;
    const float overlapRight = (wallR.x + wallR.width) - er.x;
    const float overlapTop = (er.y + er.height) - wallR.y;
    const float overlapBottom = (wallR.y + wallR.height) - er.y;

    const float minOverlap =
        std::min({overlapLeft, overlapRight, overlapTop, overlapBottom});

    if (minOverlap == overlapLeft) {
        pos.x -= overlapLeft;
    } else if (minOverlap == overlapRight) {
        pos.x += overlapRight;
    } else if (minOverlap == overlapTop) {
        pos.y -= overlapTop;
    } else {
        pos.y += overlapBottom;
    }
}

} // namespace

void wall_resolve_collisions(entt::registry &registry) {
    const auto walls = registry.view<Wall, Transform>();
    const auto interactables = registry.view<Interactable, Transform, Sprite>();
    const auto movers = registry.view<Transform, Sprite>();

    for (const auto m : movers) {
        if (registry.all_of<Projectile>(m)) {
            continue;
        }
        // Caskets / interactables are static; we only resolve against them.
        if (registry.all_of<Interactable>(m)) {
            continue;
        }
        auto &t = registry.get<Transform>(m);
        const auto &spr = registry.get<Sprite>(m);
        for (const auto w : walls) {
            const auto &wt = registry.get<Transform>(w);
            const auto &wall = registry.get<Wall>(w);
            const Rectangle wr = wallRect(wt, wall);
            separateFromWall(t.position, spr, wr);
        }

        // Treat interactable entities (caskets) as solid blocks too.
        for (const auto it : interactables) {
            if (it == m) {
                continue;
            }
            const auto &trans = registry.get<Transform>(it);
            const auto &sprIt = registry.get<Sprite>(it);
            const Rectangle ir = entityRect(trans.position, sprIt);
            separateFromWall(t.position, spr, ir);
        }
    }
}

void wall_destroy_projectiles(entt::registry &registry) {
    const auto walls = registry.view<Wall, Transform>();
    const auto projectiles = registry.view<Projectile, Transform, Sprite>();

    std::vector<entt::entity> toDestroy;
    // entt::view doesn't provide size(); we'll just let the vector grow as needed.

    for (const auto p : projectiles) {
        if (!registry.valid(p)) {
            continue;
        }
        const auto &pt = registry.get<Transform>(p);
        const auto &ps = registry.get<Sprite>(p);
        const float r = std::max(config::PROJECTILE_RADIUS,
                                  std::max(ps.width, ps.height) * 0.5F);
        const Vector2 center{pt.position.x, pt.position.y};

        for (const auto w : walls) {
            const auto &wt = registry.get<Transform>(w);
            const auto &wall = registry.get<Wall>(w);
            const Rectangle wr = wallRect(wt, wall);
            if (circleRectOverlap(center, r, wr)) {
                toDestroy.push_back(p);
                break;
            }
        }
    }

    for (const auto e : toDestroy) {
        if (registry.valid(e)) {
            registry.destroy(e);
        }
    }
}

} // namespace dreadcast::ecs
