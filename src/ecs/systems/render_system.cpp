#include "ecs/systems/render_system.hpp"

#include <algorithm>
#include <cmath>

#include <entt/entt.hpp>
#include <raylib.h>

#include "config.hpp"
#include "core/iso_utils.hpp"
#include "core/resource_manager.hpp"
#include "core/types.hpp"
#include "ecs/components.hpp"

namespace dreadcast::ecs {

namespace {
Rectangle wallRect(const Transform &t, const Wall &w) {
    return {t.position.x - w.halfW, t.position.y - w.halfH, w.halfW * 2.0F, w.halfH * 2.0F};
}

bool segmentIntersectsRect(Vector2 from, Vector2 to, Rectangle rect) {
    const float EPS = 1.0e-6F;
    Vector2 d{to.x - from.x, to.y - from.y};
    float t0 = 0.0F;
    float t1 = 1.0F;
    auto updateAxis = [&](float start, float delta, float minB, float maxB) -> bool {
        if (std::fabs(delta) < EPS) {
            return start >= minB && start <= maxB;
        }
        const float invD = 1.0F / delta;
        float tNear = (minB - start) * invD;
        float tFar = (maxB - start) * invD;
        if (tNear > tFar) {
            std::swap(tNear, tFar);
        }
        t0 = std::max(t0, tNear);
        t1 = std::min(t1, tFar);
        return t0 <= t1;
    };
    return updateAxis(from.x, d.x, rect.x, rect.x + rect.width) &&
           updateAxis(from.y, d.y, rect.y, rect.y + rect.height);
}

bool hasLineOfSight(entt::registry &registry, Vector2 from, Vector2 to) {
    const auto walls = registry.view<Wall, Transform>();
    for (const auto w : walls) {
        const auto &t = registry.get<Transform>(w);
        const auto &wall = registry.get<Wall>(w);
        if (segmentIntersectsRect(from, to, wallRect(t, wall))) {
            return false;
        }
    }
    return true;
}


Vector2 facingToWorldDir(FacingDir d) {
    switch (d) {
    case FacingDir::E:
        return {1.0F, 0.0F};
    case FacingDir::NE:
        return dreadcast::Vec2Normalize(Vector2{1.0F, -1.0F});
    case FacingDir::N:
        return {0.0F, -1.0F};
    case FacingDir::NW:
        return dreadcast::Vec2Normalize(Vector2{-1.0F, -1.0F});
    case FacingDir::W:
        return {-1.0F, 0.0F};
    case FacingDir::SW:
        return dreadcast::Vec2Normalize(Vector2{-1.0F, 1.0F});
    case FacingDir::S:
        return {0.0F, 1.0F};
    case FacingDir::SE:
        return dreadcast::Vec2Normalize(Vector2{1.0F, 1.0F});
    default:
        return {0.0F, 1.0F};
    }
}

void drawIsoDiamondFill(Vector2 worldCenter, float halfWorld, Color fill, Color outline) {
    const Vector2 p1 = dreadcast::worldToIso(Vector2{worldCenter.x - halfWorld, worldCenter.y - halfWorld});
    const Vector2 p2 = dreadcast::worldToIso(Vector2{worldCenter.x + halfWorld, worldCenter.y - halfWorld});
    const Vector2 p3 = dreadcast::worldToIso(Vector2{worldCenter.x + halfWorld, worldCenter.y + halfWorld});
    const Vector2 p4 = dreadcast::worldToIso(Vector2{worldCenter.x - halfWorld, worldCenter.y + halfWorld});
    DrawTriangle(p1, p2, p3, fill);
    DrawTriangle(p1, p3, p4, fill);
    DrawLineV(p1, p2, outline);
    DrawLineV(p2, p3, outline);
    DrawLineV(p3, p4, outline);
    DrawLineV(p4, p1, outline);
}

void drawFacingArrow(Vector2 worldCenter, FacingDir dir, float halfWorld, Color col) {
    const Vector2 c = dreadcast::worldToIso(worldCenter);
    Vector2 isoDir = dreadcast::worldToIso(facingToWorldDir(dir));
    const float ilen = dreadcast::Vec2Length(isoDir);
    if (ilen > 0.001F) {
        isoDir.x /= ilen;
        isoDir.y /= ilen;
    } else {
        return;
    }
    const float tipDist = halfWorld * 0.85F;
    const Vector2 tip = {c.x + isoDir.x * tipDist, c.y + isoDir.y * tipDist};
    const Vector2 perp = {-isoDir.y, isoDir.x};
    const float w = halfWorld * 0.35F;
    const Vector2 b1 = {c.x + isoDir.x * (tipDist * 0.35F) + perp.x * w,
                         c.y + isoDir.y * (tipDist * 0.35F) + perp.y * w};
    const Vector2 b2 = {c.x + isoDir.x * (tipDist * 0.35F) - perp.x * w,
                         c.y + isoDir.y * (tipDist * 0.35F) - perp.y * w};
    DrawTriangle(tip, b1, b2, col);
}

Color lerpTintAgitated(Color base, bool agitated) {
    if (!agitated) {
        return base;
    }
    return {static_cast<unsigned char>(std::min(255, static_cast<int>(base.r) + 55)),
            static_cast<unsigned char>(std::max(0, static_cast<int>(base.g) - 45)),
            static_cast<unsigned char>(std::max(0, static_cast<int>(base.b) - 35)), base.a};
}

Vector2 agitationShake(const Agitation *ag) {
    if (ag == nullptr || !ag->agitated) {
        return {0.0F, 0.0F};
    }
    // Disable positional jitter; agitation is communicated via tint/marker only.
    return {0.0F, 0.0F};
}

void drawIsoPlaceholder(entt::registry &registry, entt::entity entity, const Transform &transform,
                        const Sprite &sprite) {
    const float half = sprite.width * 0.5F;
    const Color fill = sprite.tint;
    const Color outline = {static_cast<unsigned char>(std::min(255, fill.r + 40)),
                           static_cast<unsigned char>(std::min(255, fill.g + 40)),
                           static_cast<unsigned char>(std::min(255, fill.b + 40)), 255};
    drawIsoDiamondFill(transform.position, half, fill, outline);
    if (const auto *f = registry.try_get<Facing>(entity)) {
        const Color arrowCol = {static_cast<unsigned char>(std::min(255, fill.r + 80)),
                                static_cast<unsigned char>(std::min(255, fill.g + 80)),
                                static_cast<unsigned char>(std::min(255, fill.b + 80)), 255};
        drawFacingArrow(transform.position, f->dir, half, arrowCol);
    }
}

void drawEnemyOverlays(entt::registry &registry, const Font &font, Vector2 fogOrigin, float fogRadius) {
    const auto view = registry.view<Enemy, Transform, Sprite, Health, NameTag>();
    for (const auto entity : view) {
        const auto &transform = view.get<Transform>(entity);
        if (!visible_to_player(registry, fogOrigin, transform.position, fogRadius)) {
            continue;
        }
        const auto &sprite = view.get<Sprite>(entity);
        const auto &health = view.get<Health>(entity);
        const auto &tag = view.get<NameTag>(entity);
        const auto *ag = registry.try_get<Agitation>(entity);
        const Vector2 shake = agitationShake(ag);
        const Vector2 visPos = {transform.position.x + shake.x, transform.position.y + shake.y};

        const Vector2 iso = dreadcast::worldToIso(visPos);
        const float half = sprite.width * 0.5F;
        const float barW = sprite.width;
        const float barH = 6.0F;
        const float barY = iso.y - half - 28.0F;
        const float x = iso.x - barW * 0.5F;
        DrawRectangle(static_cast<int>(x), static_cast<int>(barY), static_cast<int>(barW),
                      static_cast<int>(barH), {40, 20, 20, 255});
        const float ratio = health.max > 0.0F ? health.current / health.max : 0.0F;
        DrawRectangle(static_cast<int>(x), static_cast<int>(barY),
                      static_cast<int>(barW * ratio), static_cast<int>(barH), {200, 60, 60, 255});

        if (registry.all_of<StunnedState>(entity)) {
            const float stunT = static_cast<float>(GetTime());
            const float spin = stunT * 4.0F;
            const float ox = std::cosf(spin) * 3.0F;
            const float oy = std::sinf(spin * 1.3F) * 2.0F;
            const char *sym = "*";
            const float stS = 16.0F;
            const Vector2 sd = MeasureTextEx(font, sym, stS, 1.0F);
            DrawTextEx(font, sym, {iso.x - sd.x * 0.5F + ox, barY - stS - 6.0F + oy}, stS, 1.0F,
                       {230, 210, 120, 255});
        }

        const float nameSize = 16.0F;
        const Vector2 nameDim = MeasureTextEx(font, tag.name, nameSize, 1.0F);
        Vector2 namePos = {iso.x - nameDim.x * 0.5F, barY - nameSize - 4.0F};
        if (registry.all_of<StunnedState>(entity)) {
            namePos.y -= 10.0F;
        }
        DrawTextEx(font, tag.name, namePos, nameSize, 1.0F, {220, 200, 200, 255});
        if (ag != nullptr && ag->agitated) {
            const char excl = '!';
            char buf[2] = {excl, '\0'};
            const float exS = 20.0F;
            const Vector2 ed = MeasureTextEx(font, buf, exS, 1.0F);
            namePos.x = iso.x - ed.x * 0.5F;
            namePos.y = barY - nameSize - 26.0F;
            DrawTextEx(font, buf, namePos, exS, 1.0F, {255, 80, 80, 255});
        }
    }
}

void drawMeleeArc(entt::registry &registry) {
    const auto view = registry.view<Player, MeleeAttacker, Transform>();
    for (const auto entity : view) {
        const auto &melee = view.get<MeleeAttacker>(entity);
        if (!melee.isMeleeArcActive()) {
            continue;
        }
        const auto &transform = view.get<Transform>(entity);
        const Vector2 isoCenter = dreadcast::worldToIso(transform.position);
        const Vector2 wForward = {std::cosf(transform.rotation), std::sinf(transform.rotation)};
        Vector2 isoF = dreadcast::worldToIso(wForward);
        const float flen = dreadcast::Vec2Length(isoF);
        if (flen < 0.001F) {
            continue;
        }
        isoF.x /= flen;
        isoF.y /= flen;
        const float baseDeg = std::atan2f(isoF.y, isoF.x) * RAD2DEG;
        const float dur = MeleeAttacker::kSwingDuration[melee.swingIndex];
        const float u = std::clamp(melee.phaseTimer / dur, 0.0F, 1.0F);
        const float halfArc = MeleeAttacker::kArcHalfDeg[melee.swingIndex];
        const float sweepSign = (melee.swingIndex % 2 == 0) ? 1.0F : -1.0F;
        const float sweep = sweepSign * (u * 56.0F - 28.0F);
        const float start = baseDeg - halfArc + sweep;
        const float end = baseDeg + halfArc + sweep;
        const float radius = melee.range * (melee.swingIndex == 2 ? 1.08F : 1.0F);
        const float fillA = 0.22F + static_cast<float>(melee.swingIndex) * 0.06F;
        const float lineA = 0.48F + static_cast<float>(melee.swingIndex) * 0.05F;
        DrawCircleSector(isoCenter, radius, start, end, 22,
                         Fade({255, static_cast<unsigned char>(200 - melee.swingIndex * 15), 100, 255},
                              fillA));
        DrawCircleSectorLines(isoCenter, radius, start, end, 22,
                              Fade({255, static_cast<unsigned char>(230 - melee.swingIndex * 20), 140, 255},
                                   lineA));
    }
}

void drawWallEntity(const Transform &transform, const Wall &wall) {
    const float x0 = transform.position.x - wall.halfW;
    const float x1 = transform.position.x + wall.halfW;
    const float y0 = transform.position.y - wall.halfH;
    const float y1 = transform.position.y + wall.halfH;
    const Vector2 p1 = dreadcast::worldToIso(Vector2{x0, y0});
    const Vector2 p2 = dreadcast::worldToIso(Vector2{x1, y0});
    const Vector2 p3 = dreadcast::worldToIso(Vector2{x1, y1});
    const Vector2 p4 = dreadcast::worldToIso(Vector2{x0, y1});
    const Color fill = {55, 48, 42, 255};
    const Color outline = {110, 90, 75, 255};
    DrawTriangle(p1, p2, p3, fill);
    DrawTriangle(p1, p3, p4, fill);
    DrawLineV(p1, p2, outline);
    DrawLineV(p2, p3, outline);
    DrawLineV(p3, p4, outline);
    DrawLineV(p4, p1, outline);
}

void drawSpriteEntity(entt::entity entity, entt::registry &registry, ResourceManager &resources,
                      Vector2 fogOrigin, float fogRadius) {
    const auto &transform = registry.get<Transform>(entity);
    const bool shouldFogHide =
        registry.all_of<Enemy>(entity) || registry.all_of<ManaShard>(entity) ||
        (registry.all_of<Projectile>(entity) && !registry.get<Projectile>(entity).fromPlayer);
    if (shouldFogHide && !visible_to_player(registry, fogOrigin, transform.position, fogRadius)) {
        return;
    }
    const auto &sprite = registry.get<Sprite>(entity);
    const auto *agitation = registry.try_get<Agitation>(entity);
    const Vector2 shake = agitationShake(agitation);
    Transform drawTransform = transform;
    drawTransform.position.x += shake.x;
    drawTransform.position.y += shake.y;
    Sprite drawSprite = sprite;
    if (agitation != nullptr) {
        drawSprite.tint = lerpTintAgitated(sprite.tint, agitation->agitated);
    }

    if (const auto *anim = registry.try_get<SpriteAnimation>(entity)) {
        const Texture2D tex = resources.getTexture(anim->spritesheetPath);
        if (tex.id != 0 && anim->frameWidth > 0 && anim->frameHeight > 0 && anim->frameCount > 0) {
            const Vector2 iso = dreadcast::worldToIso(drawTransform.position);
            const Rectangle dst = {iso.x - drawSprite.width * 0.5F, iso.y - drawSprite.height * 0.5F,
                                   drawSprite.width, drawSprite.height};
            const Vector2 origin = {drawSprite.width * 0.5F, drawSprite.height * 0.5F};
            const float totalFrames = GetTime() * static_cast<double>(anim->fps);
            const int frame = static_cast<int>(std::floor(totalFrames)) % anim->frameCount;
            const Rectangle src = {static_cast<float>(frame * anim->frameWidth), 0.0F,
                                     static_cast<float>(anim->frameWidth),
                                     static_cast<float>(anim->frameHeight)};
            DrawTexturePro(tex, src, dst, origin, 0.0F, drawSprite.tint);
            return;
        }
    }

    if (const auto *texSprite = registry.try_get<TextureSprite>(entity)) {
        const Texture2D tex = resources.getTexture(texSprite->path);
        if (tex.id != 0) {
            const Vector2 iso = dreadcast::worldToIso(drawTransform.position);
            const Rectangle dst = {iso.x - drawSprite.width * 0.5F, iso.y - drawSprite.height * 0.5F,
                                   drawSprite.width, drawSprite.height};
            const Vector2 origin = {drawSprite.width * 0.5F, drawSprite.height * 0.5F};
            const Rectangle src = {0.0F, 0.0F, static_cast<float>(tex.width),
                                   static_cast<float>(tex.height)};
            DrawTexturePro(tex, src, dst, origin, 0.0F, drawSprite.tint);
            return;
        }
    }

    if (registry.all_of<Player, Facing>(entity) || registry.all_of<Enemy, Facing>(entity)) {
        drawIsoPlaceholder(registry, entity, drawTransform, drawSprite);
        return;
    }

    const float half = drawSprite.width * 0.5F;
    const Color fill = drawSprite.tint;
    const Color outline = {static_cast<unsigned char>(std::min(255, fill.r + 40)),
                           static_cast<unsigned char>(std::min(255, fill.g + 40)),
                           static_cast<unsigned char>(std::min(255, fill.b + 40)), 255};
    drawIsoDiamondFill(drawTransform.position, half, fill, outline);
}

} // namespace

void render_system(entt::registry &registry, const Font &font, ResourceManager &resources) {
    Vector2 fogOrigin{0.0F, 0.0F};
    for (const auto p : registry.view<Player, Transform>()) {
        fogOrigin = registry.get<Transform>(p).position;
        break;
    }
    const float fogRadius = config::FOG_OF_WAR_RADIUS;

    for (const auto w : registry.view<Wall, Transform>()) {
        drawWallEntity(registry.get<Transform>(w), registry.get<Wall>(w));
    }

    const auto view = registry.view<Transform, Sprite>();
    for (const auto entity : view) {
        if (registry.all_of<Wall>(entity)) {
            continue;
        }
        drawSpriteEntity(entity, registry, resources, fogOrigin, fogRadius);
    }

    drawEnemyOverlays(registry, font, fogOrigin, fogRadius);
    drawMeleeArc(registry);
}

bool visible_to_player(entt::registry &registry, Vector2 playerPos, Vector2 targetPos, float radius) {
    const float dx = targetPos.x - playerPos.x;
    const float dy = targetPos.y - playerPos.y;
    if (dx * dx + dy * dy > radius * radius) {
        return false;
    }
    return hasLineOfSight(registry, playerPos, targetPos);
}

} // namespace dreadcast::ecs
