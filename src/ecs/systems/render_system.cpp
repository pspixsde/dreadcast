#include "ecs/systems/render_system.hpp"

#include <algorithm>
#include <cmath>

#include <entt/entt.hpp>
#include <raylib.h>

#include "core/iso_utils.hpp"
#include "core/resource_manager.hpp"
#include "core/types.hpp"
#include "ecs/components.hpp"

namespace dreadcast::ecs {

namespace {

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
    const float t = static_cast<float>(GetTime());
    return {std::sinf(t * 25.0F) * 2.5F, std::cosf(t * 23.0F) * 2.0F};
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

void drawEnemyOverlays(entt::registry &registry, const Font &font) {
    const auto view = registry.view<Enemy, Transform, Sprite, Health, NameTag>();
    for (const auto entity : view) {
        const auto &transform = view.get<Transform>(entity);
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

        const float nameSize = 16.0F;
        const Vector2 nameDim = MeasureTextEx(font, tag.name, nameSize, 1.0F);
        Vector2 namePos = {iso.x - nameDim.x * 0.5F, barY - nameSize - 4.0F};
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
        if (!melee.isAttacking) {
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
        const float deg = std::atan2f(isoF.y, isoF.x) * RAD2DEG;
        DrawCircleSector(isoCenter, melee.range, deg - 45.0F, deg + 45.0F, 16,
                         Fade({255, 200, 120, 255}, 0.25F));
        DrawCircleSectorLines(isoCenter, melee.range, deg - 45.0F, deg + 45.0F, 16,
                              Fade({255, 220, 160, 255}, 0.55F));
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

void drawSpriteEntity(entt::entity entity, entt::registry &registry, ResourceManager &resources) {
    const auto &transform = registry.get<Transform>(entity);
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
    for (const auto w : registry.view<Wall, Transform>()) {
        drawWallEntity(registry.get<Transform>(w), registry.get<Wall>(w));
    }

    const auto view = registry.view<Transform, Sprite>();
    for (const auto entity : view) {
        if (registry.all_of<Wall>(entity)) {
            continue;
        }
        drawSpriteEntity(entity, registry, resources);
    }

    drawEnemyOverlays(registry, font);
    drawMeleeArc(registry);
}

} // namespace dreadcast::ecs
