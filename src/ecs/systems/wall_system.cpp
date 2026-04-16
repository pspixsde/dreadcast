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

void separateEntities(Vector2 &aPos, const Sprite &aSpr, Vector2 &bPos, const Sprite &bSpr) {
    const Rectangle ar = entityRect(aPos, aSpr);
    const Rectangle br = entityRect(bPos, bSpr);
    if (!CheckCollisionRecs(ar, br)) {
        return;
    }

    const float overlapLeft = (ar.x + ar.width) - br.x;
    const float overlapRight = (br.x + br.width) - ar.x;
    const float overlapTop = (ar.y + ar.height) - br.y;
    const float overlapBottom = (br.y + br.height) - ar.y;

    const float minOverlap = std::min({overlapLeft, overlapRight, overlapTop, overlapBottom});
    const float push = minOverlap * 0.5F;
    if (minOverlap == overlapLeft) {
        aPos.x -= push;
        bPos.x += push;
    } else if (minOverlap == overlapRight) {
        aPos.x += push;
        bPos.x -= push;
    } else if (minOverlap == overlapTop) {
        aPos.y -= push;
        bPos.y += push;
    } else {
        aPos.y += push;
        bPos.y -= push;
    }
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

[[nodiscard]] bool aabbOverlapsPolygonFill(const Vector2 &pos, const Sprite &spr,
                                           const std::vector<Vector2> &poly) {
    const int n = static_cast<int>(poly.size());
    if (n < 3) {
        return false;
    }
    const Rectangle er = entityRect(pos, spr);
    const Vector2 corners[4] = {
        {er.x, er.y},
        {er.x + er.width, er.y},
        {er.x + er.width, er.y + er.height},
        {er.x, er.y + er.height},
    };
    for (const Vector2 &c : corners) {
        if (CheckCollisionPointPoly(c, poly.data(), n)) {
            return true;
        }
    }
    const Vector2 mid{er.x + er.width * 0.5F, er.y + er.height * 0.5F};
    if (CheckCollisionPointPoly(mid, poly.data(), n)) {
        return true;
    }
    return false;
}

void separateFromSolidPolygon(Vector2 &pos, const Sprite &spr,
                              const std::vector<Vector2> &poly) {
    const int n = static_cast<int>(poly.size());
    if (n < 3 || !aabbOverlapsPolygonFill(pos, spr, poly)) {
        return;
    }
    Vector2 centroid{0.0F, 0.0F};
    for (const Vector2 &v : poly) {
        centroid.x += v.x;
        centroid.y += v.y;
    }
    const float inv = 1.0F / static_cast<float>(n);
    centroid.x *= inv;
    centroid.y *= inv;

    for (int iter = 0; iter < 48; ++iter) {
        if (!aabbOverlapsPolygonFill(pos, spr, poly)) {
            return;
        }
        float dx = pos.x - centroid.x;
        float dy = pos.y - centroid.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1.0e-3F) {
            pos.x += 4.0F;
            continue;
        }
        dx /= len;
        dy /= len;
        pos.x += dx * 4.0F;
        pos.y += dy * 4.0F;
    }
}

[[nodiscard]] bool circleHitsPolygon(Vector2 center, float r, const std::vector<Vector2> &poly) {
    const int n = static_cast<int>(poly.size());
    if (n < 3) {
        return false;
    }
    if (CheckCollisionPointPoly(center, poly.data(), n)) {
        return true;
    }
    for (int i = 0; i < n; ++i) {
        const Vector2 a = poly[static_cast<size_t>(i)];
        const Vector2 b = poly[static_cast<size_t>((i + 1) % n)];
        if (CheckCollisionCircleLine(center, r, a, b)) {
            return true;
        }
    }
    return false;
}

} // namespace

void wall_resolve_collisions(entt::registry &registry) {
    const auto walls = registry.view<Wall, Transform>();
    const auto polys = registry.view<SolidPolygon, Transform>();
    const auto interactables = registry.view<Interactable, Transform, Sprite>();
    const auto movers = registry.view<Transform, Sprite>();

    for (const auto m : movers) {
        if (registry.all_of<Projectile>(m)) {
            continue;
        }
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

        for (const auto pe : polys) {
            const auto &poly = registry.get<SolidPolygon>(pe);
            separateFromSolidPolygon(t.position, spr, poly.vertsWorld);
        }

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

void unit_resolve_collisions(entt::registry &registry) {
    std::vector<entt::entity> units;
    const auto view = registry.view<Transform, Sprite>();
    for (const auto e : view) {
        const bool isUnit = registry.all_of<Player>(e) || registry.all_of<Enemy>(e);
        if (!isUnit || registry.all_of<Projectile>(e) || registry.all_of<Interactable>(e)) {
            continue;
        }
        units.push_back(e);
    }

    for (size_t i = 0; i < units.size(); ++i) {
        for (size_t j = i + 1; j < units.size(); ++j) {
            const entt::entity a = units[i];
            const entt::entity b = units[j];
            if (!registry.valid(a) || !registry.valid(b)) {
                continue;
            }
            auto &ta = registry.get<Transform>(a);
            auto &tb = registry.get<Transform>(b);
            const auto &sa = registry.get<Sprite>(a);
            const auto &sb = registry.get<Sprite>(b);
            separateEntities(ta.position, sa, tb.position, sb);
        }
    }
}

void wall_destroy_projectiles(entt::registry &registry) {
    const auto walls = registry.view<Wall, Transform>();
    const auto polys = registry.view<SolidPolygon, Transform>();
    const auto projectiles = registry.view<Projectile, Transform, Sprite>();

    std::vector<entt::entity> toDestroy;

    for (const auto p : projectiles) {
        if (!registry.valid(p)) {
            continue;
        }
        const auto &pt = registry.get<Transform>(p);
        const auto &ps = registry.get<Sprite>(p);
        const float r = std::max(config::PROJECTILE_RADIUS,
                                  std::max(ps.width, ps.height) * 0.5F);
        const Vector2 center{pt.position.x, pt.position.y};

        bool hit = false;
        for (const auto w : walls) {
            const auto &wt = registry.get<Transform>(w);
            const auto &wall = registry.get<Wall>(w);
            const Rectangle wr = wallRect(wt, wall);
            if (circleRectOverlap(center, r, wr)) {
                hit = true;
                break;
            }
        }
        if (!hit) {
            for (const auto pe : polys) {
                const auto &poly = registry.get<SolidPolygon>(pe);
                if (circleHitsPolygon(center, r, poly.vertsWorld)) {
                    hit = true;
                    break;
                }
            }
        }
        if (hit) {
            toDestroy.push_back(p);
        }
    }

    for (const auto e : toDestroy) {
        if (registry.valid(e)) {
            registry.destroy(e);
        }
    }
}

} // namespace dreadcast::ecs
