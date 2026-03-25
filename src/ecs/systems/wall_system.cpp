#include "ecs/systems/wall_system.hpp"

#include <algorithm>
#include <cmath>

#include <entt/entt.hpp>
#include <raylib.h>

#include "ecs/components.hpp"

namespace dreadcast::ecs {

namespace {

Rectangle wallRect(const Transform &t, const Wall &w) {
    return {t.position.x - w.halfW, t.position.y - w.halfH, w.halfW * 2.0F, w.halfH * 2.0F};
}

Rectangle entityRect(const Vector2 &pos, const Sprite &s) {
    return {pos.x - s.width * 0.5F, pos.y - s.height * 0.5F, s.width, s.height};
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
    const auto movers = registry.view<Transform, Sprite>();

    for (const auto m : movers) {
        if (registry.all_of<Projectile>(m)) {
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
    }
}

} // namespace dreadcast::ecs
