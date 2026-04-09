#include "scenes/gameplay_scene.hpp"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>

#include <raylib.h>
#include <rlgl.h>

#include "ui/theme.hpp"

#include "config.hpp"
#include "core/fog_visibility.hpp"
#include "core/cursor_draw.hpp"
#include "core/input.hpp"
#include "core/iso_utils.hpp"
#include "core/resource_manager.hpp"
#include "ecs/components.hpp"
#include "ecs/systems/collision_system.hpp"
#include "ecs/systems/combat_system.hpp"
#include "ecs/systems/death_system.hpp"
#include "ecs/systems/enemy_ai_system.hpp"
#include "ecs/systems/input_system.hpp"
#include "ecs/systems/movement_system.hpp"
#include "ecs/systems/projectile_system.hpp"
#include "ecs/systems/render_system.hpp"
#include "ecs/systems/wall_system.hpp"
#include "game/ability.hpp"
#include "game/character.hpp"
#include "game/item_factory.hpp"
#include "game/map_data.hpp"
#include "scenes/menu_scene.hpp"
#include "scenes/scene_manager.hpp"
#include "scenes/settings_scene.hpp"

namespace dreadcast {

namespace {

// Single-texture overlay: samples fog mask (R=white fog). Outputs black with alpha so default
// alpha-blend darkens the framebuffer like mix(scene, black, m*fogStrength) without needing a
// second sampler (Raylib's batch only reliably binds texture0 for DrawTexturePro).
// Optional 5x5 blur of the mask softens the visibility edge (Dota-like falloff).
static const char *kFogOverlayFs = R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float fogStrength;
uniform float fogEdgeSoftPx;
void main() {
    float m;
    if (fogEdgeSoftPx < 0.5) {
        m = texture(texture0, fragTexCoord).r;
    } else {
        vec2 ts = 1.0 / vec2(textureSize(texture0, 0));
        vec2 delta = (fogEdgeSoftPx * 0.25) * ts;
        m = 0.0;
        for (int j = -2; j <= 2; ++j) {
            for (int i = -2; i <= 2; ++i) {
                m += texture(texture0, fragTexCoord + vec2(float(i), float(j)) * delta).r;
            }
        }
        m *= 0.04;
    }
    float a = clamp(m * fogStrength, 0.0, 1.0);
    finalColor = vec4(0.0, 0.0, 0.0, a) * colDiffuse * fragColor;
}
)";

void spawnWallBlock(entt::registry &reg, float cx, float cy, float halfW, float halfH) {
    const auto e = reg.create();
    reg.emplace<ecs::Transform>(e, ecs::Transform{{cx, cy}, 0.0F});
    reg.emplace<ecs::Wall>(e, ecs::Wall{halfW, halfH});
}

/// Filled convex visibility polygon: center at fan[0], boundary fan[1..n] in CCW order around the
/// center (same as DrawTriangleFan). Uses per-triangle DrawTriangle calls so we never rely on one
/// huge RL_QUADS batch (more reliable across drivers than a single DrawTriangleFan).
static void drawFogVisibilityFilled(const Vector2 *fan, int nBoundary, Color color) {
    if (nBoundary < 3) {
        return;
    }
    const Vector2 &c = fan[0];
    for (int i = 0; i < nBoundary; ++i) {
        const Vector2 &a = fan[1 + i];
        const Vector2 &b = fan[1 + ((i + 1) % nBoundary)];
        // 2D cross (a-c) x (b-c); sign picks a front-facing winding for the current Y-down space.
        const float cross = (a.x - c.x) * (b.y - c.y) - (a.y - c.y) * (b.x - c.x);
        if (cross < 0.0F) {
            DrawTriangle(c, b, a, color);
        } else {
            DrawTriangle(c, a, b, color);
        }
    }
}

void clampRectToScreen(Rectangle &r, int screenW, int screenH) {
    if (r.x < 4.0F) {
        r.x = 4.0F;
    }
    if (r.y < 4.0F) {
        r.y = 4.0F;
    }
    if (r.x + r.width > static_cast<float>(screenW) - 4.0F) {
        r.x = static_cast<float>(screenW) - r.width - 4.0F;
    }
    if (r.y + r.height > static_cast<float>(screenH) - 4.0F) {
        r.y = static_cast<float>(screenH) - r.height - 4.0F;
    }
}

} // namespace

GameplayScene::GameplayScene(int selectedClassIndex) : selectedClass_(selectedClassIndex) {}

GameplayScene::~GameplayScene() { unloadFogResources(); }

void GameplayScene::onEnter() {
    camera_.init(config::WINDOW_WIDTH, config::WINDOW_HEIGHT);
    initFogResources();
    if (!spawned_) {
        spawnWorld();
        spawned_ = true;
    }
}

void GameplayScene::onExit() { unloadFogResources(); }

void GameplayScene::initFogResources() {
    if (fogResourcesReady_) {
        return;
    }
    fogSceneTarget_ = LoadRenderTexture(config::WINDOW_WIDTH, config::WINDOW_HEIGHT);
    // Same dimensions as the scene RT so fog mask UVs match texture0 in the composite shader.
    fogMaskTarget_ = LoadRenderTexture(config::WINDOW_WIDTH, config::WINDOW_HEIGHT);
    if (!IsRenderTextureValid(fogSceneTarget_) || !IsRenderTextureValid(fogMaskTarget_)) {
        if (IsRenderTextureValid(fogSceneTarget_)) {
            UnloadRenderTexture(fogSceneTarget_);
        }
        if (IsRenderTextureValid(fogMaskTarget_)) {
            UnloadRenderTexture(fogMaskTarget_);
        }
        fogSceneTarget_ = {};
        fogMaskTarget_ = {};
        return;
    }
    SetTextureFilter(fogSceneTarget_.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(fogMaskTarget_.texture, TEXTURE_FILTER_BILINEAR);

    fogCompositeShader_ = LoadShaderFromMemory(nullptr, kFogOverlayFs);
    if (!IsShaderValid(fogCompositeShader_)) {
        UnloadRenderTexture(fogSceneTarget_);
        UnloadRenderTexture(fogMaskTarget_);
        fogSceneTarget_ = {};
        fogMaskTarget_ = {};
        return;
    }
    fogOverlayLocStrength_ = GetShaderLocation(fogCompositeShader_, "fogStrength");
    fogOverlayLocEdgeSoft_ = GetShaderLocation(fogCompositeShader_, "fogEdgeSoftPx");
    if (fogOverlayLocStrength_ < 0 || fogOverlayLocEdgeSoft_ < 0) {
        UnloadShader(fogCompositeShader_);
        UnloadRenderTexture(fogSceneTarget_);
        UnloadRenderTexture(fogMaskTarget_);
        fogCompositeShader_ = {};
        fogSceneTarget_ = {};
        fogMaskTarget_ = {};
        fogOverlayLocStrength_ = -1;
        fogOverlayLocEdgeSoft_ = -1;
        return;
    }
    fogResourcesReady_ = true;
}

void GameplayScene::unloadFogResources() {
    if (IsRenderTextureValid(fogSceneTarget_)) {
        UnloadRenderTexture(fogSceneTarget_);
    }
    if (IsRenderTextureValid(fogMaskTarget_)) {
        UnloadRenderTexture(fogMaskTarget_);
    }
    if (IsShaderValid(fogCompositeShader_)) {
        UnloadShader(fogCompositeShader_);
    }
    fogSceneTarget_ = {};
    fogMaskTarget_ = {};
    fogCompositeShader_ = {};
    fogOverlayLocStrength_ = -1;
    fogOverlayLocEdgeSoft_ = -1;
    fogResourcesReady_ = false;
}

void GameplayScene::drawWorldContent(ResourceManager &resources) {
    const int gridExtent = 2000;
    const int step = 64;
    const int tiles = gridExtent / step;
    const Color gridCol = {55, 32, 32, 255};
    for (int i = -tiles; i <= tiles; ++i) {
        const float x = static_cast<float>(i * step);
        const Vector2 a = worldToIso(Vector2{x, static_cast<float>(-gridExtent)});
        const Vector2 b = worldToIso(Vector2{x, static_cast<float>(gridExtent)});
        DrawLineV(a, b, gridCol);
    }
    for (int j = -tiles; j <= tiles; ++j) {
        const float y = static_cast<float>(j * step);
        const Vector2 a = worldToIso(Vector2{static_cast<float>(-gridExtent), y});
        const Vector2 b = worldToIso(Vector2{static_cast<float>(gridExtent), y});
        DrawLineV(a, b, gridCol);
    }

    ecs::render_system(registry_, resources.uiFont(), resources);

    if (registry_.valid(player_) && registry_.all_of<ecs::Transform>(player_)) {
        const Vector2 pWorld = registry_.get<ecs::Transform>(player_).position;
        const Vector2 pIso = worldToIso(pWorld);
        const float t = static_cast<float>(GetTime());

        if (registry_.all_of<ecs::HealOverTime>(player_)) {
            const auto &hot = registry_.get<ecs::HealOverTime>(player_);
            const float tRem = hot.duration > 0.001F
                                   ? std::max(0.0F, 1.0F - hot.elapsed / hot.duration)
                                   : 0.0F;
            const float pulse = 0.6F + 0.4F * std::sinf(t * 3.0F);
            const float baseR = 30.0F + pulse * 8.0F;
            DrawCircleV(pIso, baseR, Fade({180, 40, 40, 255}, 0.08F * tRem));
            DrawCircleLinesV(pIso, baseR, Fade({220, 70, 50, 255}, 0.3F * pulse * tRem));

            for (int i = 0; i < 3; ++i) {
                const float speed = 2.5F + static_cast<float>(i) * 1.3F;
                const float orbitR = 22.0F + static_cast<float>(i) * 5.0F;
                const float angle = t * speed + static_cast<float>(i) * 2.094F;
                const Vector2 dot{pIso.x + std::cosf(angle) * orbitR,
                                  pIso.y + std::sinf(angle) * orbitR * 0.5F};
                DrawCircleV(dot, 2.5F, Fade({255, 100, 80, 255}, 0.6F * tRem));
            }
        }

        if (registry_.all_of<ecs::LeadFeverEffect>(player_)) {
            const auto &lf = registry_.get<ecs::LeadFeverEffect>(player_);
            const float tRem = lf.duration > 0.001F
                                   ? std::max(0.0F, 1.0F - lf.elapsed / lf.duration)
                                   : 0.0F;
            const float pulse = 0.55F + 0.45F * std::sinf(t * 5.0F);
            const float gr = 24.0F + pulse * 6.0F;
            DrawCircleV(pIso, gr, Fade({60, 200, 120, 255}, 0.07F * tRem));
            DrawCircleLinesV(pIso, gr, Fade({120, 255, 160, 255}, 0.22F * pulse * tRem));
        }

        if (registry_.all_of<ecs::ManicEffect>(player_)) {
            const auto &me = registry_.get<ecs::ManicEffect>(player_);
            const float tRem = me.duration > 0.001F
                                   ? std::max(0.0F, 1.0F - me.elapsed / me.duration)
                                   : 0.0F;
            const float flicker = 0.5F + 0.5F * std::sinf(t * 12.0F);
            const float auraR = 28.0F + flicker * 6.0F;
            DrawCircleV(pIso, auraR, Fade({200, 160, 50, 255}, 0.06F * tRem));
            for (int ring = 0; ring < 2; ++ring) {
                const float rr = auraR + static_cast<float>(ring) * 8.0F;
                const float a = 0.25F * (1.0F - static_cast<float>(ring) * 0.4F) * flicker * tRem;
                DrawCircleLinesV(pIso, rr, Fade({255, 210, 80, 255}, a));
            }

            if (registry_.all_of<ecs::Velocity>(player_)) {
                const auto &vel = registry_.get<ecs::Velocity>(player_);
                const float speed = std::sqrt(vel.value.x * vel.value.x + vel.value.y * vel.value.y);
                if (speed > 10.0F) {
                    const Vector2 velDir{-vel.value.x / speed, -vel.value.y / speed};
                    const Vector2 velIso = worldToIso(Vector2{velDir.x, velDir.y});
                    const float vLen = std::sqrt(velIso.x * velIso.x + velIso.y * velIso.y);
                    const Vector2 trailDir = vLen > 0.001F
                                                 ? Vector2{velIso.x / vLen, velIso.y / vLen}
                                                 : Vector2{0.0F, -1.0F};
                    for (int s = 0; s < 4; ++s) {
                        const float offset = static_cast<float>(s) * 7.0F + 8.0F;
                        const float spread = (static_cast<float>(s % 2) - 0.5F) * 10.0F;
                        const Vector2 perp{-trailDir.y, trailDir.x};
                        const Vector2 a{pIso.x + trailDir.x * offset + perp.x * spread,
                                        pIso.y + trailDir.y * offset + perp.y * spread};
                        const Vector2 b{a.x + trailDir.x * 14.0F, a.y + trailDir.y * 14.0F};
                        const float la = 0.25F * tRem * (1.0F - static_cast<float>(s) * 0.15F);
                        DrawLineEx(a, b, 2.0F, Fade({255, 220, 100, 255}, la));
                    }
                }
            }
        }

        if (registry_.all_of<ecs::SnareDashState>(player_) && registry_.all_of<ecs::Velocity>(player_)) {
            const auto &vel = registry_.get<ecs::Velocity>(player_);
            const float speed = std::sqrt(vel.value.x * vel.value.x + vel.value.y * vel.value.y);
            if (speed > 40.0F) {
                const Vector2 velDir{-vel.value.x / speed, -vel.value.y / speed};
                const Vector2 velIso = worldToIso(Vector2{velDir.x, velDir.y});
                const float vLen = std::sqrt(velIso.x * velIso.x + velIso.y * velIso.y);
                const Vector2 trailDir = vLen > 0.001F ? Vector2{velIso.x / vLen, velIso.y / vLen}
                                                       : Vector2{0.0F, -1.0F};
                for (int s = 0; s < 3; ++s) {
                    const float offset = static_cast<float>(s) * 6.0F + 6.0F;
                    const Vector2 a{pIso.x + trailDir.x * offset, pIso.y + trailDir.y * offset};
                    const Vector2 b{a.x + trailDir.x * 10.0F, a.y + trailDir.y * 10.0F};
                    DrawLineEx(a, b, 2.0F, Fade({160, 150, 230, 255}, 0.35F));
                }
            }
        }
    }

    if (snareImpactFlash_ > 0.001F) {
        const Vector2 isoHit = worldToIso(snareImpactWorld_);
        const float pr = std::min(1.0F, snareImpactFlash_ / 0.55F);
        for (int k = 0; k < 6; ++k) {
            const float ang = static_cast<float>(k) * 1.0471976F + static_cast<float>(GetTime()) * 3.0F;
            const float rr = 18.0F + static_cast<float>(k) * 5.0F;
            const Vector2 a{isoHit.x + std::cosf(ang) * rr, isoHit.y + std::sinf(ang) * rr * 0.5F};
            DrawCircleV(a, 3.0F, Fade({180, 160, 255, 255}, 0.5F * pr));
        }
        DrawCircleLinesV(isoHit, 40.0F * pr, Fade({200, 180, 255, 255}, 0.35F * pr));
    }

    if (registry_.valid(player_) && registry_.all_of<ecs::SlugAimState, ecs::Transform>(player_)) {
        const auto &pt = registry_.get<ecs::Transform>(player_);
        const auto &aim = registry_.get<ecs::SlugAimState>(player_);
        const Vector2 pIso = worldToIso(pt.position);
        const Vector2 d = aim.aimDirection;
        const float len = std::sqrt(d.x * d.x + d.y * d.y);
        const Vector2 nd = len > 0.001F ? Vector2{d.x / len, d.y / len} : Vector2{1.0F, 0.0F};
        const Vector2 wEnd{pt.position.x + nd.x * 120.0F, pt.position.y + nd.y * 120.0F};
        const Vector2 eIso = worldToIso(wEnd);
        const float pulse = 0.5F + 0.5F * std::sinf(static_cast<float>(GetTime()) * 10.0F);
        DrawLineEx(pIso, eIso, 2.0F, Fade({255, 220, 120, 255}, 0.35F + 0.25F * pulse));
    }

    for (const auto pe :
         registry_.view<ecs::Projectile, ecs::SlugProjectile, ecs::Transform, ecs::Sprite>()) {
        const auto &pt = registry_.get<ecs::Transform>(pe);
        const Vector2 iso = worldToIso(pt.position);
        const float pulse = 0.5F + 0.5F * std::sinf(static_cast<float>(GetTime()) * 14.0F);
        DrawCircleV(iso, 6.0F, Fade({255, 200, 80, 255}, 0.25F * pulse));
    }

    if (runicShellFlashTimer_ > 0.001F && registry_.valid(player_) &&
        registry_.all_of<ecs::Transform>(player_)) {
        const Vector2 pWorld = registry_.get<ecs::Transform>(player_).position;
        const Vector2 pIso = worldToIso(pWorld);
        const float progress = 1.0F - runicShellFlashTimer_ / 0.6F;
        const float ringR = config::RUNIC_SHELL_RADIUS * progress;
        const float alpha = std::max(0.0F, 1.0F - progress);
        for (int r = 0; r < 3; ++r) {
            const float rr = ringR - static_cast<float>(r) * 6.0F;
            if (rr > 0.0F) {
                const float thick = 4.0F - static_cast<float>(r) * 1.0F;
                const float a = alpha * (1.0F - static_cast<float>(r) * 0.25F);
                DrawRing(pIso, rr - thick * 0.5F, rr + thick * 0.5F, 0.0F, 360.0F, 60,
                         Fade({130, 180, 255, 255}, a));
            }
        }
        DrawCircleV(pIso, ringR * 0.3F, Fade({200, 220, 255, 255}, alpha * 0.15F));
    }

    drawLootPickupHighlight(resources);
}

void GameplayScene::spawnWalls(const MapData &map) {
    for (const WallData &w : map.walls) {
        spawnWallBlock(registry_, w.cx, w.cy, w.halfW, w.halfH);
    }
}

void GameplayScene::spawnWorld() {
    registry_.clear();
    inventory_ = InventoryState{};
    statusHudOrder_.clear();
    for (float &cd : abilityCdRem_) {
        cd = 0.0F;
    }
    snareImpactFlash_ = 0.0F;
    MapData map = defaultMapData();
    MapData loaded;
    if (loaded.loadFromFile("assets/maps/default.map")) {
        map = loaded;
    }

    player_ = registry_.create();
    registry_.emplace<ecs::Transform>(player_, ecs::Transform{map.playerSpawn, 0.0F});
    registry_.emplace<ecs::Velocity>(player_, ecs::Velocity{});
    registry_.emplace<ecs::Sprite>(player_,
                                   ecs::Sprite{{0, 220, 255, 255}, 36.0F, 36.0F});
    registry_.emplace<ecs::Health>(player_, ecs::Health{config::PLAYER_BASE_MAX_HP,
                                                        config::PLAYER_BASE_MAX_HP});
    registry_.emplace<ecs::Mana>(player_, ecs::Mana{100.0F, 100.0F});
    registry_.emplace<ecs::MeleeAttacker>(player_, ecs::MeleeAttacker{});
    registry_.emplace<ecs::Facing>(player_, ecs::Facing{});
    registry_.emplace<ecs::Player>(player_);

    spawnWalls(map);

    for (const EnemySpawnData &p : map.enemies) {
        const auto e = registry_.create();
        registry_.emplace<ecs::Transform>(e, ecs::Transform{{p.x, p.y}, 0.0F});
        registry_.emplace<ecs::Velocity>(e, ecs::Velocity{});
        const bool hellhound = (p.type == "hellhound");
        const Color tint = hellhound ? Color{160, 70, 40, 255} : Color{220, 90, 60, 255};
        const float size =
            hellhound ? config::HELLHOUND_SPRITE_SIZE : config::IMP_SPRITE_SIZE;
        registry_.emplace<ecs::Sprite>(e, ecs::Sprite{tint, size, size});
        registry_.emplace<ecs::Facing>(e, ecs::Facing{});
        const float hp = hellhound ? config::HELLHOUND_HP : 50.0F;
        registry_.emplace<ecs::Health>(e, ecs::Health{hp, hp});
        registry_.emplace<ecs::Enemy>(e);
        registry_.emplace<ecs::NameTag>(e, ecs::NameTag{hellhound ? "Hellhound" : "Imp"});
        registry_.emplace<ecs::EnemyAI>(e, ecs::EnemyAI{
                                               hellhound ? ecs::EnemyType::Hellhound
                                                         : ecs::EnemyType::Imp,
                                               config::IMP_SHOOT_COOLDOWN,
                                               config::IMP_SHOOT_COOLDOWN,
                                               config::IMP_MIN_SHOOT_RANGE,
                                               hellhound ? config::HELLHOUND_DAMAGE : 0.0F,
                                               hellhound ? config::HELLHOUND_MELEE_RANGE : 0.0F,
                                               config::HELLHOUND_MELEE_COOLDOWN,
                                               0.0F,
                                               hellhound ? config::HELLHOUND_CHASE_SPEED : 0.0F});
        registry_.emplace<ecs::Agitation>(
            e, ecs::Agitation{hellhound ? config::HELLHOUND_AGITATION_RANGE
                                        : config::IMP_AGITATION_RANGE,
                              config::ENEMY_CALM_DOWN_DELAY, 0.0F, false});
    }

    if (map.hasCasket) {
        const auto casket = registry_.create();
        registry_.emplace<ecs::Transform>(casket, ecs::Transform{map.casketPos, 0.0F});
        registry_.emplace<ecs::Velocity>(casket, ecs::Velocity{});
        registry_.emplace<ecs::Sprite>(casket, ecs::Sprite{{90, 70, 55, 255}, 56.0F, 40.0F});
        registry_.emplace<ecs::Interactable>(casket, ecs::Interactable{"Old Casket", false});
    }

    for (const ItemSpawnData &isp : map.itemSpawns) {
        ItemData it = makeItemFromMapKind(isp.kind);
        if (it.name.empty()) {
            continue;
        }
        const int idx = inventory_.addItem(std::move(it));
        spawnItemPickupAtWorld({isp.x, isp.y}, idx);
    }

    prevPlayerHp_ = config::PLAYER_BASE_MAX_HP;
}

Vector2 GameplayScene::worldMouseFromScreen(const Vector2 &screenMouse) const {
    const Vector2 isoMouse = GetScreenToWorld2D(screenMouse, camera_.camera());
    return isoToWorld(isoMouse);
}

void GameplayScene::applyPlayerMaxHpFromEquipment() {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Health>(player_)) {
        return;
    }
    auto &hp = registry_.get<ecs::Health>(player_);
    const float bonus = inventory_.totalEquippedMaxHpBonus();
    const float newMax = config::PLAYER_BASE_MAX_HP + bonus;
    if (std::fabs(hp.max - newMax) < 0.001F) {
        return;
    }
    const float hpRatio = hp.max > 0.001F ? hp.current / hp.max : 1.0F;
    hp.max = newMax;
    hp.current = std::clamp(hpRatio * hp.max, 0.0F, hp.max);
    // Max-HP scaling changes are not incoming damage events.
    prevPlayerHp_ = hp.current;
}

void GameplayScene::tickHealOverTime(float fixedDt) {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Health>(player_)) {
        return;
    }
    if (registry_.all_of<ecs::ManicEffect>(player_)) {
        return;
    }
    if (!registry_.all_of<ecs::HealOverTime>(player_)) {
        return;
    }
    auto &hp = registry_.get<ecs::Health>(player_);
    auto &hot = registry_.get<ecs::HealOverTime>(player_);
    const float rate = hot.totalHeal / hot.duration;
    float add = rate * fixedDt;
    const float remainingHeal = hot.totalHeal - hot.healedSoFar;
    if (add > remainingHeal) {
        add = remainingHeal;
    }
    hot.healedSoFar += add;
    hot.elapsed += fixedDt;
    hp.current = std::min(hp.max, hp.current + add);
    if (hot.elapsed >= hot.duration - 1.0e-4F || hot.healedSoFar >= hot.totalHeal - 1.0e-3F) {
        registry_.remove<ecs::HealOverTime>(player_);
    }
}

void GameplayScene::tryUseConsumableSlot(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= CONSUMABLE_SLOT_COUNT) {
        return;
    }
    const int idx = inventory_.consumableSlots[static_cast<size_t>(slotIndex)];
    if (idx < 0 || idx >= static_cast<int>(inventory_.items.size())) {
        return;
    }
    ItemData &it = inventory_.items[static_cast<size_t>(idx)];
    if (!it.isConsumable) {
        return;
    }
    if (it.name == "Vial of Pure Blood") {
        const bool wasHot =
            registry_.valid(player_) && registry_.all_of<ecs::HealOverTime>(player_);
        it.stackCount -= 1;
        if (it.stackCount <= 0) {
            inventory_.consumableSlots[static_cast<size_t>(slotIndex)] = -1;
            inventory_.removeItemAtIndex(idx);
        }
        applyVialHealOverTime(wasHot);
        return;
    }
    if (it.name == "Vial of Cordial Manic") {
        if (!tryApplyCordialManic()) {
            return;
        }
        it.stackCount -= 1;
        if (it.stackCount <= 0) {
            inventory_.consumableSlots[static_cast<size_t>(slotIndex)] = -1;
            inventory_.removeItemAtIndex(idx);
        }
    }
}

void GameplayScene::tryUseConsumableBagSlot(int bagSlot) {
    if (bagSlot < 0 || bagSlot >= dreadcast::BAG_SLOT_COUNT) {
        return;
    }
    const int idx = inventory_.bagSlots[static_cast<size_t>(bagSlot)];
    if (idx < 0 || idx >= static_cast<int>(inventory_.items.size())) {
        return;
    }
    ItemData &it = inventory_.items[static_cast<size_t>(idx)];
    if (!it.isConsumable) {
        return;
    }
    if (it.name == "Vial of Pure Blood") {
        const bool wasHot =
            registry_.valid(player_) && registry_.all_of<ecs::HealOverTime>(player_);
        it.stackCount -= 1;
        if (it.stackCount <= 0) {
            inventory_.bagSlots[static_cast<size_t>(bagSlot)] = -1;
            inventory_.removeItemAtIndex(idx);
        }
        applyVialHealOverTime(wasHot);
        return;
    }
    if (it.name == "Vial of Cordial Manic") {
        if (!tryApplyCordialManic()) {
            return;
        }
        it.stackCount -= 1;
        if (it.stackCount <= 0) {
            inventory_.bagSlots[static_cast<size_t>(bagSlot)] = -1;
            inventory_.removeItemAtIndex(idx);
        }
    }
}

void GameplayScene::applyVialHealOverTime(bool wasAlreadyActive) {
    if (!registry_.valid(player_)) {
        return;
    }
    if (wasAlreadyActive) {
        hotRefreshFlashTimer_ = 0.35F;
    }
    registry_.emplace_or_replace<ecs::HealOverTime>(
        player_, ecs::HealOverTime{config::HOT_TOTAL_HEAL, config::HOT_DURATION, 0.0F, 0.0F});
    pushStatusHud(StatusHudKind::HealOverTime, "assets/textures/items/vial_pure_blood_icon.png",
                  {220, 80, 80, 255});
}

bool GameplayScene::tryApplyCordialManic() {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Health>(player_)) {
        return false;
    }
    auto &hp = registry_.get<ecs::Health>(player_);
    if (hp.max > 0.001F &&
        hp.current + 1.0e-4F < config::MANIC_MIN_HP_FRACTION * hp.max) {
        return false;
    }
    const float drainTotal = hp.max * config::MANIC_HP_DRAIN_PERCENT;
    registry_.emplace_or_replace<ecs::ManicEffect>(
        player_, ecs::ManicEffect{config::MANIC_DURATION, 0.0F, drainTotal, 0.0F});
    pushStatusHud(StatusHudKind::ManicEffect, "assets/textures/items/vial_cordial_manic_icon.png",
                  {255, 210, 120, 255});
    return true;
}

void GameplayScene::tickManicEffect(float fixedDt) {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::ManicEffect, ecs::Health>(player_)) {
        return;
    }
    auto &me = registry_.get<ecs::ManicEffect>(player_);
    auto &hp = registry_.get<ecs::Health>(player_);
    me.elapsed += fixedDt;
    const float rate = me.duration > 0.001F ? me.hpDrainTotal / me.duration : 0.0F;
    float add = rate * fixedDt;
    const float remaining = me.hpDrainTotal - me.hpDrained;
    if (add > remaining) {
        add = remaining;
    }
    if (add > 0.0F) {
        me.hpDrained += add;
        hp.current = std::max(0.0F, hp.current - add);
    }
    if (me.elapsed >= me.duration - 1.0e-4F ||
        me.hpDrained >= me.hpDrainTotal - 1.0e-3F) {
        registry_.remove<ecs::ManicEffect>(player_);
    }
}

void GameplayScene::tickRunicShellCooldown(float fixedDt) {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::RunicShellCooldown>(player_)) {
        return;
    }
    auto &cd = registry_.get<ecs::RunicShellCooldown>(player_);
    cd.remaining -= fixedDt;
    if (cd.remaining <= 0.0F) {
        registry_.remove<ecs::RunicShellCooldown>(player_);
    }
}

void GameplayScene::checkRunicShellTrigger() {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Health, ecs::Transform>(player_)) {
        return;
    }
    const int armorIdx = inventory_.equipped[static_cast<size_t>(EquipSlot::Armor)];
    if (armorIdx < 0 || armorIdx >= static_cast<int>(inventory_.items.size())) {
        return;
    }
    if (inventory_.items[static_cast<size_t>(armorIdx)].name != "Runic Shell") {
        return;
    }
    if (registry_.all_of<ecs::RunicShellCooldown>(player_)) {
        return;
    }
    const auto &hp = registry_.get<ecs::Health>(player_);
    if (hp.max < 0.001F || hp.current > config::RUNIC_SHELL_HP_THRESHOLD * hp.max) {
        return;
    }

    const auto &pt = registry_.get<ecs::Transform>(player_);
    const float radiusSq = config::RUNIC_SHELL_RADIUS * config::RUNIC_SHELL_RADIUS;
    const auto enemies = registry_.view<ecs::Enemy, ecs::Transform, ecs::Health>();
    for (const auto e : enemies) {
        const auto &et = registry_.get<ecs::Transform>(e);
        const float dx = et.position.x - pt.position.x;
        const float dy = et.position.y - pt.position.y;
        if (dx * dx + dy * dy > radiusSq) {
            continue;
        }
        auto &eh = registry_.get<ecs::Health>(e);
        eh.current -= config::RUNIC_SHELL_DAMAGE;

        const float dist = std::sqrt(dx * dx + dy * dy);
        if (dist > 0.001F) {
            auto &vel = registry_.get_or_emplace<ecs::Velocity>(e);
            vel.value.x = (dx / dist) * config::RUNIC_SHELL_KNOCKBACK;
            vel.value.y = (dy / dist) * config::RUNIC_SHELL_KNOCKBACK;
            registry_.emplace_or_replace<ecs::KnockbackState>(
                e, ecs::KnockbackState{config::KNOCKBACK_DURATION * 1.5F, 0.0F});
        }
    }

    auto &hpMut = registry_.get<ecs::Health>(player_);
    if (!registry_.all_of<ecs::ManicEffect>(player_)) {
        hpMut.current = std::min(hpMut.max, hpMut.current + config::RUNIC_SHELL_HEAL);
    }
    registry_.emplace_or_replace<ecs::RunicShellCooldown>(
        player_, ecs::RunicShellCooldown{config::RUNIC_SHELL_COOLDOWN, config::RUNIC_SHELL_COOLDOWN});
    runicShellFlashTimer_ = 0.6F;
}

void GameplayScene::spawnItemPickupAtPlayer(int itemIndex) {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Transform>(player_)) {
        return;
    }
    const auto &t = registry_.get<ecs::Transform>(player_);
    spawnItemPickupAtWorld(t.position, itemIndex);
}

void GameplayScene::spawnItemPickupAtWorld(const Vector2 &worldPos, int itemIndex) {
    Vector2 dropPos = worldPos;
    constexpr float minSep = 20.0F; // keep multiple drops pickable
    const float minSepSq = minSep * minSep;

    // Nudge the drop position outward if it would overlap an existing ground pickup.
    for (int attempt = 0; attempt < 24; ++attempt) {
        bool ok = true;
        const auto pickups = registry_.view<ecs::ItemPickup, ecs::Transform>();
        for (const auto p : pickups) {
            const auto &pt = pickups.get<ecs::Transform>(p);
            const float dx = pt.position.x - dropPos.x;
            const float dy = pt.position.y - dropPos.y;
            if (dx * dx + dy * dy < minSepSq) {
                ok = false;
                break;
            }
        }
        if (ok) {
            break;
        }
        const float ang = attempt * 2.3999632F; // golden angle (radians)
        const float r = minSep * (0.55F + 0.25F * static_cast<float>(attempt));
        dropPos = {worldPos.x + std::cosf(ang) * r, worldPos.y + std::sinf(ang) * r};
    }

    const auto e = registry_.create();
    registry_.emplace<ecs::Transform>(e, ecs::Transform{dropPos, 0.0F});
    registry_.emplace<ecs::Velocity>(e, ecs::Velocity{});
    registry_.emplace<ecs::Sprite>(e, ecs::Sprite{{180, 120, 255, 255}, 28.0F, 28.0F});
    registry_.emplace<ecs::ItemPickup>(e, ecs::ItemPickup{itemIndex});
}

void GameplayScene::update(SceneManager &scenes, InputManager &input, ResourceManager &resources,
                           float frameDt) {
    if (input.isKeyPressed(KEY_F3)) {
        fogDebugOverlay_ = !fogDebugOverlay_;
    }

    noManaFlashTimer_ = std::max(0.0F, noManaFlashTimer_ - frameDt);
    inventoryFullFlashTimer_ = std::max(0.0F, inventoryFullFlashTimer_ - frameDt);
    damageFlashTimer_ = std::max(0.0F, damageFlashTimer_ - frameDt);
    hotRefreshFlashTimer_ = std::max(0.0F, hotRefreshFlashTimer_ - frameDt);
    runicShellFlashTimer_ = std::max(0.0F, runicShellFlashTimer_ - frameDt);
    snareImpactFlash_ = std::max(0.0F, snareImpactFlash_ - frameDt);

    if (gameOver_) {
        const Vector2 mouse = input.mousePosition();
        const bool click = input.isMouseButtonPressed(MOUSE_BUTTON_LEFT);
        const int w = config::WINDOW_WIDTH;
        const int h = config::WINDOW_HEIGHT;
        retryButton_.rect = {(w - 280.0F) * 0.5F, h * 0.5F + 40.0F, 280.0F, 48.0F};
        retryButton_.label = "Retry";
        gameOverMenuButton_.rect = {(w - 280.0F) * 0.5F, h * 0.5F + 105.0F, 280.0F, 48.0F};
        gameOverMenuButton_.label = "Main Menu";
        if (retryButton_.wasClicked(mouse, click)) {
            scenes.replace(std::make_unique<GameplayScene>(selectedClass_));
        }
        if (gameOverMenuButton_.wasClicked(mouse, click)) {
            scenes.replace(std::make_unique<MenuScene>(selectedClass_));
        }
        hoveredPickup_ = entt::null;
        hoveredInteract_ = entt::null;
        return;
    }

    if (input.isKeyPressed(KEY_TAB)) {
        if (!paused_) {
            const bool wasOpen = inventoryUi_.isOpen();
            inventoryUi_.toggle();
            if (wasOpen && !inventoryUi_.isOpen()) {
                aimScreenPos_ = input.mousePosition();
            }
        }
    }

    if (inventoryUi_.isOpen()) {
        if (input.isKeyPressed(KEY_ESCAPE)) {
            inventoryUi_.setOpen(false);
            aimScreenPos_ = input.mousePosition();
        } else {
            const ui::InventoryAction invAction = inventoryUi_.update(input, inventory_);
            if (invAction.type == ui::InventoryAction::Drop) {
                spawnItemPickupAtPlayer(invAction.itemIndex);
            } else if (invAction.type == ui::InventoryAction::Use) {
                if (invAction.useConsumableSlot >= 0) {
                    tryUseConsumableSlot(invAction.useConsumableSlot);
                } else if (invAction.useBagSlot >= 0) {
                    tryUseConsumableBagSlot(invAction.useBagSlot);
                }
            }
        }
    }

    applyPlayerMaxHpFromEquipment();

    if (!inventoryUi_.isOpen() && input.isKeyPressed(KEY_ESCAPE)) {
        paused_ = !paused_;
    }

    if (paused_) {
        const Vector2 mouse = input.mousePosition();
        const bool click = input.isMouseButtonPressed(MOUSE_BUTTON_LEFT);
        const int w = config::WINDOW_WIDTH;
        const int h = config::WINDOW_HEIGHT;
        resumeButton_.rect = {(w - 260.0F) * 0.5F, h * 0.5F - 95.0F, 260.0F, 48.0F};
        resumeButton_.label = "Resume";
        settingsPauseButton_.rect = {(w - 260.0F) * 0.5F, h * 0.5F - 30.0F, 260.0F, 48.0F};
        settingsPauseButton_.label = "Settings";
        mainMenuButton_.rect = {(w - 260.0F) * 0.5F, h * 0.5F + 35.0F, 260.0F, 48.0F};
        mainMenuButton_.label = "Main Menu";
        if (resumeButton_.wasClicked(mouse, click)) {
            paused_ = false;
        }
        if (settingsPauseButton_.wasClicked(mouse, click)) {
            scenes.push(std::make_unique<SettingsScene>());
        }
        if (mainMenuButton_.wasClicked(mouse, click)) {
            scenes.replace(std::make_unique<MenuScene>(selectedClass_));
        }
        hoveredPickup_ = entt::null;
        hoveredInteract_ = entt::null;
        return;
    }

    aimMouseSensitivity_ = resources.settings().mouseSensitivity;
    if (!aimScreenInit_) {
        aimScreenPos_ = input.mousePosition();
        aimScreenInit_ = true;
    }
    if (!paused_ && !inventoryUi_.isOpen()) {
        const Vector2 d = input.mouseDelta();
        aimScreenPos_.x = std::clamp(aimScreenPos_.x + d.x * aimMouseSensitivity_, 0.0F,
                                     static_cast<float>(config::WINDOW_WIDTH));
        aimScreenPos_.y = std::clamp(aimScreenPos_.y + d.y * aimMouseSensitivity_, 0.0F,
                                     static_cast<float>(config::WINDOW_HEIGHT));
    }

    const bool invOpen = inventoryUi_.isOpen();
    if (!invOpen) {
        ecs::input_system(registry_, input, camera_.camera(), aimScreenPos_);
        ecs::combat_player_ranged(registry_, input, camera_.camera(), player_, noManaFlashTimer_,
                                  aimScreenPos_);
    } else if (registry_.valid(player_) && registry_.all_of<ecs::Velocity>(player_)) {
        auto &vel = registry_.get<ecs::Velocity>(player_);
        vel.value.x = 0.0F;
        vel.value.y = 0.0F;
    }

    const int steps = fixedTimer_.consumeSteps(frameDt);
    for (int i = 0; i < steps; ++i) {
        tickStunnedEnemies(config::FIXED_DT);
        tickAbilityCooldowns(config::FIXED_DT);
        tickLeadFeverEffect(config::FIXED_DT);
        tickSnareDash(config::FIXED_DT);
        tickSlugAim(config::FIXED_DT);
        if (!invOpen) {
            ecs::combat_player_melee(registry_, input, player_, config::FIXED_DT);
        } else if (registry_.valid(player_) && registry_.all_of<ecs::MeleeAttacker>(player_)) {
            auto &melee = registry_.get<ecs::MeleeAttacker>(player_);
            melee.phase = ecs::MeleeAttacker::Phase::Idle;
            melee.phaseTimer = 0.0F;
            melee.swingIndex = 0;
            melee.hitAppliedThisSwing = false;
        }
        ecs::enemy_ai_system(registry_, config::FIXED_DT, player_, &inventory_);
        ecs::movement_system(registry_, config::FIXED_DT);
        ecs::wall_resolve_collisions(registry_);
        ecs::unit_resolve_collisions(registry_);
        ecs::wall_destroy_projectiles(registry_);
        ecs::projectile_system(registry_, config::FIXED_DT);
        ecs::collision::projectile_hits(registry_, player_, &inventory_, &snareImpactWorld_,
                                        &snareImpactFlash_);
        ecs::collision::player_pickup_mana_shards(registry_, player_);
        ecs::death_system(registry_, player_, &enemiesSlain_);
        tickManicEffect(config::FIXED_DT);
        tickHealOverTime(config::FIXED_DT);
        tickRunicShellCooldown(config::FIXED_DT);
        checkRunicShellTrigger();

        // Passive regen based on the selected class (+ equipment); blocked during ManicEffect.
        if (registry_.valid(player_) && registry_.all_of<ecs::Health, ecs::Mana>(player_) &&
            !registry_.all_of<ecs::ManicEffect>(player_)) {
            const int ci = std::clamp(selectedClass_, 0, CLASS_COUNT - 1);
            const auto &cls = AVAILABLE_CLASSES[static_cast<size_t>(ci)];
            auto &hp2 = registry_.get<ecs::Health>(player_);
            auto &mp = registry_.get<ecs::Mana>(player_);
            hp2.current = std::min(
                hp2.max, hp2.current + cls.hpRegen * config::FIXED_DT +
                             inventory_.totalEquippedHpRegenBonus() * config::FIXED_DT);
            mp.current = std::min(mp.max, mp.current + cls.manaRegen * config::FIXED_DT);
        } else if (registry_.valid(player_) && registry_.all_of<ecs::Mana>(player_) &&
                   registry_.all_of<ecs::ManicEffect>(player_)) {
            auto &mp = registry_.get<ecs::Mana>(player_);
            const int ci = std::clamp(selectedClass_, 0, CLASS_COUNT - 1);
            const auto &cls = AVAILABLE_CLASSES[static_cast<size_t>(ci)];
            mp.current = std::min(mp.max, mp.current + cls.manaRegen * config::FIXED_DT);
        }
    }

    if (registry_.valid(player_) && registry_.all_of<ecs::Health>(player_)) {
        const float cur = registry_.get<ecs::Health>(player_).current;
        if (cur < prevPlayerHp_ - 1.0e-4F) {
            damageFlashTimer_ = 0.5F;
        }
        prevPlayerHp_ = cur;
    }

    if (!invOpen && !paused_) {
        if (input.isKeyPressed(KEY_C)) {
            tryUseConsumableSlot(0);
        }
        if (input.isKeyPressed(KEY_V)) {
            tryUseConsumableSlot(1);
        }
        if (input.isKeyPressed(KEY_ONE)) {
            tryUseAbility(0);
        }
        if (input.isKeyPressed(KEY_TWO)) {
            tryUseAbility(1);
        }
        if (input.isKeyPressed(KEY_THREE)) {
            tryUseAbility(2);
        }
    }

    if (!invOpen) {
        if (registry_.valid(player_) && registry_.all_of<ecs::Transform>(player_)) {
            const Vector2 ppos = registry_.get<ecs::Transform>(player_).position;
            const Vector2 wm = worldMouseFromScreen(aimScreenPos_);
            hoveredPickup_ = ecs::collision::find_item_pickup_hover_in_range(
                registry_, ppos, wm, config::LOOT_PICKUP_RANGE);
            if (!registry_.valid(hoveredPickup_)) {
                hoveredPickup_ = entt::null;
            }
            hoveredInteract_ = ecs::collision::find_interactable_hover_in_range(
                registry_, ppos, wm, config::INTERACT_RANGE);
            if (!registry_.valid(hoveredInteract_)) {
                hoveredInteract_ = entt::null;
            }

            if (input.isKeyPressed(KEY_F) && hoveredInteract_ != entt::null &&
                registry_.all_of<ecs::Interactable>(hoveredInteract_)) {
                auto &inter = registry_.get<ecs::Interactable>(hoveredInteract_);
                if (!inter.opened && inter.name == "Old Casket") {
                    inter.opened = true;
                    const int armorIdx = inventory_.addItem(makeItemFromMapKind("iron_armor"));
                    const int vileIdx = inventory_.addItem(makeItemFromMapKind("vial_pure_blood"));

                    const auto &ct = registry_.get<ecs::Transform>(hoveredInteract_);
                    const auto &casketSpr = registry_.get<ecs::Sprite>(hoveredInteract_);

                    Vector2 dir = {ppos.x - ct.position.x, ppos.y - ct.position.y};
                    const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                    if (len > 0.001F) {
                        dir.x /= len;
                        dir.y /= len;
                    } else {
                        dir = {1.0F, 0.0F};
                    }
                    const Vector2 perp = {-dir.y, dir.x};

                    const float half = std::max(casketSpr.width, casketSpr.height) * 0.5F;
                    const float baseDist = half + 12.0F;
                    const float spread = 12.0F;

                    const Vector2 dropA = {ct.position.x + dir.x * baseDist + perp.x * spread,
                                             ct.position.y + dir.y * baseDist + perp.y * spread};
                    const Vector2 dropB = {ct.position.x + dir.x * baseDist - perp.x * spread,
                                             ct.position.y + dir.y * baseDist - perp.y * spread};
                    spawnItemPickupAtWorld(dropA, armorIdx);
                    spawnItemPickupAtWorld(dropB, vileIdx);
                }
            }

            if (input.isKeyPressed(KEY_E) && hoveredPickup_ != entt::null) {
                ecs::collision::try_pickup_item_entity(registry_, hoveredPickup_, inventory_,
                                                       inventoryFullFlashTimer_);
                hoveredPickup_ = entt::null;
            }
        }
    } else {
        hoveredPickup_ = entt::null;
        hoveredInteract_ = entt::null;
    }

    if (registry_.valid(player_) && registry_.all_of<ecs::Transform>(player_)) {
        const auto &t = registry_.get<ecs::Transform>(player_);
        camera_.setFollowTarget(worldToIso(t.position));
    }
    camera_.update(frameDt);

    if (registry_.valid(player_) && registry_.all_of<ecs::Health>(player_)) {
        if (registry_.get<ecs::Health>(player_).current <= 0.0F) {
            gameOver_ = true;
            paused_ = false;
        }
    }
}

void GameplayScene::draw(ResourceManager &resources) {
    const int w = config::WINDOW_WIDTH;
    const int h = config::WINDOW_HEIGHT;

    if (fogResourcesReady_) {
        fogDbgLegacyPath_ = false;
        BeginTextureMode(fogSceneTarget_);
        ClearBackground(ui::theme::CLEAR_BG);
        BeginMode2D(camera_.camera());
        drawWorldContent(resources);
        EndMode2D();
        EndTextureMode();

        drawFogMaskTexture();
        drawFogCompositePass();
    } else {
        fogDbgLegacyPath_ = true;
        fogDbgMaskSkipped_ = true;
        fogDbgBoundarySamples_ = 0;
        fogDbgFanVerts_ = 0;
        DrawRectangle(0, 0, w, h, ui::theme::CLEAR_BG);
        BeginMode2D(camera_.camera());
        drawWorldContent(resources);
        EndMode2D();
        drawFogOfWarLegacy();
    }

    drawDamageVignette();
    drawHud(resources);
    drawFlashMessages(resources);

    float hpRatioDraw = 1.0F;
    if (registry_.valid(player_) && registry_.all_of<ecs::Health>(player_)) {
        const auto &hp = registry_.get<ecs::Health>(player_);
        hpRatioDraw = hp.max > 0.001F ? hp.current / hp.max : 0.0F;
    }
    float rscdRatio = 0.0F;
    float rscdSeconds = 0.0F;
    if (registry_.valid(player_) && registry_.all_of<ecs::RunicShellCooldown>(player_)) {
        const auto &cd = registry_.get<ecs::RunicShellCooldown>(player_);
        rscdRatio = cd.total > 0.001F ? cd.remaining / cd.total : 0.0F;
        rscdSeconds = cd.remaining;
    }
    inventoryUi_.draw(resources.uiFont(), resources, w, h, inventory_, hpRatioDraw,
                      rscdRatio, rscdSeconds);

    drawLootProximityPrompt(resources);

    if (paused_) {
        drawPauseOverlay(resources);
    }
    if (gameOver_) {
        drawGameOverScreen(resources);
    }

    if (fogDebugOverlay_) {
        drawFogDebugOverlay(resources.uiFont());
    }
}

void GameplayScene::drawFogMaskTexture() {
    if (!IsTextureValid(fogMaskTarget_.texture)) {
        fogDbgMaskSkipped_ = true;
        return;
    }

    const Camera2D cam = camera_.camera();

    if (!registry_.valid(player_) || !registry_.all_of<ecs::Transform>(player_)) {
        BeginTextureMode(fogMaskTarget_);
        ClearBackground(BLACK);
        EndTextureMode();
        fogDbgMaskSkipped_ = false;
        fogDbgBoundarySamples_ = 0;
        fogDbgFanVerts_ = 0;
        return;
    }

    const auto &pt = registry_.get<ecs::Transform>(player_);
    const Vector2 playerWorld = pt.position;

    buildVisibilityPolygonWorld(playerWorld, registry_, config::FOG_OF_WAR_RADIUS, fogVisWorld_);

    const int n = static_cast<int>(fogVisWorld_.size());
    fogDbgBoundarySamples_ = n;
    const Vector2 centerIso = worldToIso(playerWorld);

    // Fan in world ray order. Vertices must be in the same space as drawWorldContent (isometric
    // camera space): projecting with GetWorldToScreen2D into a CPU Image misaligned the mask
    // relative to the scene rendered into fogSceneTarget_ (different projection path).
    fogVisFan_.clear();
    fogVisFan_.reserve(static_cast<size_t>(n) + 3U);
    fogVisFan_.push_back(centerIso);
    for (int i = 0; i < n; ++i) {
        fogVisFan_.push_back(worldToIso(fogVisWorld_[static_cast<size_t>(i)]));
    }
    BeginTextureMode(fogMaskTarget_);
    ClearBackground(WHITE);
    // Defensive: UI / other code can leave scissor on; mask would clip to nothing.
    rlDisableScissorTest();
    rlDisableDepthTest();
    BeginMode2D(cam);

    // rlgl enables GL_CULL_FACE at init; triangulation below fixes winding per triangle.
    rlDisableBackfaceCulling();
    if (n >= 3) {
        drawFogVisibilityFilled(fogVisFan_.data(), n, BLACK);
    } else {
        DrawCircleV(centerIso, config::FOG_OF_WAR_RADIUS, BLACK);
    }
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
    // Leave depth test off (matches rlgl default for 2D); do not enable here.

    fogDbgFanVerts_ = static_cast<int>(fogVisFan_.size());
    fogDbgMaskSkipped_ = false;

    EndMode2D();
    EndTextureMode();
}

void GameplayScene::drawFogCompositePass() {
    const float strength =
        static_cast<float>(config::FOG_DARKNESS_ALPHA) / 255.0F;
    const float edgeSoft = config::FOG_EDGE_SOFTEN_PIXELS;
    const Rectangle dst = {0.0F, 0.0F, static_cast<float>(config::WINDOW_WIDTH),
                           static_cast<float>(config::WINDOW_HEIGHT)};
    const Rectangle srcScene = {0.0F, 0.0F, static_cast<float>(fogSceneTarget_.texture.width),
                                -static_cast<float>(fogSceneTarget_.texture.height)};
    const Rectangle srcMask = {0.0F, 0.0F, static_cast<float>(fogMaskTarget_.texture.width),
                               -static_cast<float>(fogMaskTarget_.texture.height)};
    // Pass 1: world (default shader) — only texture0 is used; guaranteed binding.
    DrawTexturePro(fogSceneTarget_.texture, srcScene, dst, Vector2{0.0F, 0.0F}, 0.0F, WHITE);
    // Pass 2: fog mask as alpha-only overlay — same DrawTexture path, single sampler.
    SetShaderValue(fogCompositeShader_, fogOverlayLocStrength_, &strength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(fogCompositeShader_, fogOverlayLocEdgeSoft_, &edgeSoft, SHADER_UNIFORM_FLOAT);
    BeginShaderMode(fogCompositeShader_);
    DrawTexturePro(fogMaskTarget_.texture, srcMask, dst, Vector2{0.0F, 0.0F}, 0.0F, WHITE);
    EndShaderMode();
}

void GameplayScene::drawFogDebugOverlay(const Font &font) {
    constexpr float textSz = 16.0F;
    constexpr float lineH = 20.0F;
    constexpr float previewW = 220.0F;
    constexpr float previewH = 124.0F;
    constexpr float margin = 10.0F;

    const float tx = margin;
    float ty = margin;
    char buf[320];

    DrawRectangle(0, 0, 440, 210, Color{0, 0, 0, 170});

    snprintf(buf, sizeof(buf), "[Fog debug] F3 toggles this panel");
    DrawTextEx(font, buf, {tx, ty}, textSz, 1.0F, RAYWHITE);
    ty += lineH;

    snprintf(buf, sizeof(buf), "Path: %s | fogResourcesReady=%d",
             fogDbgLegacyPath_ ? "LEGACY" : "RT+shader", fogResourcesReady_ ? 1 : 0);
    DrawTextEx(font, buf, {tx, ty}, textSz, 1.0F, YELLOW);
    ty += lineH;

    snprintf(buf, sizeof(buf), "Mask draw skipped: %d | boundaryPts=%d fanVerts=%d",
             fogDbgMaskSkipped_ ? 1 : 0, fogDbgBoundarySamples_, fogDbgFanVerts_);
    DrawTextEx(font, buf, {tx, ty}, textSz, 1.0F, SKYBLUE);
    ty += lineH;

    const float strength =
        static_cast<float>(config::FOG_DARKNESS_ALPHA) / 255.0F;
    snprintf(buf, sizeof(buf),
             "Composite strength=%.3f (alpha %u) | edgeSoftPx=%.1f | locs S=%d E=%d", strength,
             static_cast<unsigned>(config::FOG_DARKNESS_ALPHA), config::FOG_EDGE_SOFTEN_PIXELS,
             fogOverlayLocStrength_, fogOverlayLocEdgeSoft_);
    DrawTextEx(font, buf, {tx, ty}, textSz, 1.0F, LIGHTGRAY);
    ty += lineH;

    DrawTextEx(font, "Mask: BLACK=visible hole, WHITE=fog (before composite)", {tx, ty}, 14.0F,
               1.0F, Color{200, 200, 200, 255});
    ty += lineH;

    if (fogResourcesReady_ && IsTextureValid(fogSceneTarget_.texture) &&
        IsTextureValid(fogMaskTarget_.texture)) {
        const float px =
            static_cast<float>(config::WINDOW_WIDTH) - previewW - margin;
        float py = margin;

        const Rectangle srcScene = {
            0.0F, 0.0F, static_cast<float>(fogSceneTarget_.texture.width),
            -static_cast<float>(fogSceneTarget_.texture.height)};
        const Rectangle dstScene = {px, py, previewW, previewH};
        DrawTextEx(font, "fogSceneTarget (world RT)", {px, py - 18.0F}, 14.0F, 1.0F, LIME);
        DrawTexturePro(fogSceneTarget_.texture, srcScene, dstScene, Vector2{0.0F, 0.0F}, 0.0F,
                       WHITE);
        DrawRectangleLines(static_cast<int>(px), static_cast<int>(py), static_cast<int>(previewW),
                           static_cast<int>(previewH), LIME);

        py += previewH + 22.0F;
        const Rectangle srcMask = {0.0F, 0.0F, static_cast<float>(fogMaskTarget_.texture.width),
                                   -static_cast<float>(fogMaskTarget_.texture.height)};
        const Rectangle dstMask = {px, py, previewW, previewH};
        DrawTextEx(font, "fogMaskTarget (alpha mask)", {px, py - 18.0F}, 14.0F, 1.0F, ORANGE);
        DrawTexturePro(fogMaskTarget_.texture, srcMask, dstMask, Vector2{0.0F, 0.0F}, 0.0F, WHITE);
        DrawRectangleLines(static_cast<int>(px), static_cast<int>(py), static_cast<int>(previewW),
                           static_cast<int>(previewH), ORANGE);
    } else {
        DrawTextEx(font, "RT previews N/A (fog init failed or invalid textures)", {tx, ty}, textSz,
                   1.0F, RED);
    }
}

void GameplayScene::drawCursor(ResourceManager &resources) {
    const Vector2 m = (!paused_ && !gameOver_ && !inventoryUi_.isOpen() && aimScreenInit_)
                          ? aimScreenPos_
                          : GetMousePosition();
    if (gameOver_ || paused_ || inventoryUi_.isOpen()) {
        drawCustomCursor(resources, CursorKind::Default, m);
        return;
    }
    if (registry_.valid(hoveredInteract_) || registry_.valid(hoveredPickup_)) {
        drawCustomCursor(resources, CursorKind::Interact, m);
        return;
    }
    drawCustomCursor(resources, CursorKind::Aim, m);
}

void GameplayScene::drawFogOfWarLegacy() {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Transform>(player_)) {
        return;
    }
    // Fallback when fog RenderTexture/shader init failed (GPU / driver).
    const auto &pt = registry_.get<ecs::Transform>(player_);
    const Vector2 center =
        GetWorldToScreen2D(worldToIso(pt.position), camera_.camera());
    const float inner = config::FOG_OF_WAR_RADIUS * camera_.camera().zoom;
    const float outer = std::sqrt(static_cast<float>(config::WINDOW_WIDTH * config::WINDOW_WIDTH +
                                                      config::WINDOW_HEIGHT * config::WINDOW_HEIGHT));
    if (outer <= inner + 1.0F) {
        return;
    }
    DrawRing(center, inner, outer, 0.0F, 360.0F, 120,
             {0, 0, 0, config::FOG_DARKNESS_ALPHA});
}

void GameplayScene::drawDamageVignette() {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Health>(player_)) {
        return;
    }
    const auto &hp = registry_.get<ecs::Health>(player_);
    const float w = static_cast<float>(config::WINDOW_WIDTH);
    const float h = static_cast<float>(config::WINDOW_HEIGHT);
    const float lowFrac = hp.max > 0.001F ? hp.current / hp.max : 0.0F;
    const float lowHpBoost = lowFrac <= 0.2F ? 0.35F : 0.0F;
    const float flashBoost = std::min(1.0F, damageFlashTimer_ * 2.0F) * 0.45F;
    const float intensity = std::min(1.0F, lowHpBoost + flashBoost);
    if (intensity <= 0.001F) {
        return;
    }
    const unsigned char a = static_cast<unsigned char>(intensity * 200.0F);
    const Color edge{200, 30, 30, a};
    const Color mid{200, 30, 30, 0};
    const float band = 220.0F;
    DrawRectangleGradientV(0, 0, static_cast<int>(w), static_cast<int>(band), edge, mid);
    DrawRectangleGradientV(0, static_cast<int>(h - band), static_cast<int>(w), static_cast<int>(band),
                           mid, edge);
    DrawRectangleGradientH(0, 0, static_cast<int>(band), static_cast<int>(h), edge, mid);
    DrawRectangleGradientH(static_cast<int>(w - band), 0, static_cast<int>(band), static_cast<int>(h),
                           mid, edge);
}

void GameplayScene::syncStatusHudOrder() {
    if (!registry_.valid(player_)) {
        statusHudOrder_.clear();
        return;
    }
    statusHudOrder_.erase(
        std::remove_if(statusHudOrder_.begin(), statusHudOrder_.end(),
                       [&](const ActiveStatusHud &h) {
                           switch (h.kind) {
                           case StatusHudKind::HealOverTime:
                               return !registry_.all_of<ecs::HealOverTime>(player_);
                           case StatusHudKind::ManicEffect:
                               return !registry_.all_of<ecs::ManicEffect>(player_);
                           case StatusHudKind::LeadFever:
                               return !registry_.all_of<ecs::LeadFeverEffect>(player_);
                           }
                           return true;
                       }),
        statusHudOrder_.end());
}

void GameplayScene::removeStatusHudKind(StatusHudKind kind) {
    statusHudOrder_.erase(std::remove_if(statusHudOrder_.begin(), statusHudOrder_.end(),
                                       [&](const ActiveStatusHud &h) { return h.kind == kind; }),
                         statusHudOrder_.end());
}

void GameplayScene::pushStatusHud(StatusHudKind kind, std::string iconPath, Color outline) {
    removeStatusHudKind(kind);
    statusHudOrder_.push_back(
        ActiveStatusHud{kind, std::move(iconPath), outline});
}

Vector2 GameplayScene::playerAimDirectionWorld() const {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Transform>(player_)) {
        return {1.0F, 0.0F};
    }
    const float r = registry_.get<ecs::Transform>(player_).rotation;
    return {std::cosf(r), std::sinf(r)};
}

void GameplayScene::tickAbilityCooldowns(float fixedDt) {
    for (float &cd : abilityCdRem_) {
        cd = std::max(0.0F, cd - fixedDt);
    }
}

void GameplayScene::tickLeadFeverEffect(float fixedDt) {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::LeadFeverEffect>(player_)) {
        return;
    }
    auto &lf = registry_.get<ecs::LeadFeverEffect>(player_);
    lf.elapsed += fixedDt;
    if (lf.elapsed >= lf.duration - 1.0e-4F) {
        registry_.remove<ecs::LeadFeverEffect>(player_);
    }
}

void GameplayScene::tickSnareDash(float fixedDt) {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::SnareDashState, ecs::Velocity>(player_)) {
        return;
    }
    auto &dash = registry_.get<ecs::SnareDashState>(player_);
    auto &vel = registry_.get<ecs::Velocity>(player_);
    vel.value.x = dash.direction.x * dash.speed;
    vel.value.y = dash.direction.y * dash.speed;
    const float step = dash.speed * fixedDt;
    dash.traveled += step;
    if (dash.traveled >= dash.distance - 1.0e-3F) {
        registry_.remove<ecs::SnareDashState>(player_);
        vel.value.x = 0.0F;
        vel.value.y = 0.0F;
    }
}

void GameplayScene::tickSlugAim(float fixedDt) {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::SlugAimState>(player_)) {
        return;
    }
    auto &aim = registry_.get<ecs::SlugAimState>(player_);
    aim.elapsed += fixedDt;
    aim.aimDirection = playerAimDirectionWorld();
    if (aim.elapsed >= aim.aimDuration - 1.0e-4F) {
        const Vector2 d = aim.aimDirection;
        registry_.remove<ecs::SlugAimState>(player_);
        fireCalamitySlug(d);
    }
}

void GameplayScene::tickStunnedEnemies(float fixedDt) {
    std::vector<entt::entity> toClear;
    for (const auto e : registry_.view<ecs::StunnedState>()) {
        auto &st = registry_.get<ecs::StunnedState>(e);
        st.elapsed += fixedDt;
        if (st.elapsed >= st.duration - 1.0e-4F) {
            toClear.push_back(e);
        }
    }
    for (const auto e : toClear) {
        if (registry_.valid(e)) {
            registry_.remove<ecs::StunnedState>(e);
        }
    }
}

void GameplayScene::spawnSnareProjectile(const Vector2 &dirWorld) {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Transform, ecs::Sprite>(player_)) {
        return;
    }
    const auto &pt = registry_.get<ecs::Transform>(player_);
    const auto &ps = registry_.get<ecs::Sprite>(player_);
    const float dx = dirWorld.x;
    const float dy = dirWorld.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    const Vector2 d = len > 0.001F ? Vector2{dx / len, dy / len} : Vector2{1.0F, 0.0F};
    const float halfDiag =
        std::sqrt(ps.width * ps.width + ps.height * ps.height) * 0.5F;
    const float spawnDist = halfDiag + config::PROJECTILE_RADIUS + 2.0F;
    const auto proj = registry_.create();
    registry_.emplace<ecs::Transform>(
        proj, ecs::Transform{{pt.position.x + d.x * spawnDist, pt.position.y + d.y * spawnDist},
                             std::atan2f(d.y, d.x)});
    registry_.emplace<ecs::Velocity>(
        proj, ecs::Velocity{{d.x * config::SNARE_THROW_SPEED, d.y * config::SNARE_THROW_SPEED}});
    registry_.emplace<ecs::Sprite>(proj, ecs::Sprite{{90, 85, 140, 255}, 14.0F, 14.0F});
    registry_.emplace<ecs::Projectile>(
        proj, ecs::Projectile{0.0F, config::SNARE_THROW_SPEED, config::SNARE_THROW_RANGE, 0.0F, d,
                              true, entt::null, false, 0.0F});
    registry_.emplace<ecs::SnareProjectile>(
        proj, ecs::SnareProjectile{config::SNARE_PULL_RADIUS, config::SNARE_STUN_DURATION});
}

void GameplayScene::fireCalamitySlug(const Vector2 &dirWorld) {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Transform, ecs::Sprite>(player_)) {
        return;
    }
    const auto &pt = registry_.get<ecs::Transform>(player_);
    const auto &ps = registry_.get<ecs::Sprite>(player_);
    const float dx = dirWorld.x;
    const float dy = dirWorld.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    const Vector2 d = len > 0.001F ? Vector2{dx / len, dy / len} : Vector2{1.0F, 0.0F};
    const float halfDiag =
        std::sqrt(ps.width * ps.width + ps.height * ps.height) * 0.5F;
    const float spawnDist = halfDiag + config::SLUG_SIZE * 0.5F + 4.0F;
    const auto proj = registry_.create();
    registry_.emplace<ecs::Transform>(
        proj, ecs::Transform{{pt.position.x + d.x * spawnDist, pt.position.y + d.y * spawnDist},
                             std::atan2f(d.y, d.x)});
    registry_.emplace<ecs::Velocity>(
        proj, ecs::Velocity{{d.x * config::SLUG_SPEED, d.y * config::SLUG_SPEED}});
    registry_.emplace<ecs::Sprite>(proj, ecs::Sprite{{255, 200, 80, 255}, config::SLUG_SIZE,
                                                     config::SLUG_SIZE});
    registry_.emplace<ecs::Projectile>(
        proj, ecs::Projectile{config::SLUG_DAMAGE, config::SLUG_SPEED, config::SLUG_MAX_RANGE, 0.0F,
                              d, true, entt::null, true, 0.0F});
    registry_.emplace<ecs::SlugProjectile>(
        proj, ecs::SlugProjectile{config::SLUG_KNOCKBACK_SIDE});
    registry_.emplace<ecs::PierceHitRecord>(proj, ecs::PierceHitRecord{});
}

void GameplayScene::tryUseAbility(int abilityIndex) {
    if (abilityIndex < 0 || abilityIndex > 2) {
        return;
    }
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Mana>(player_)) {
        return;
    }
    if (registry_.all_of<ecs::SlugAimState>(player_) ||
        registry_.all_of<ecs::SnareDashState>(player_)) {
        return;
    }
    const int ci = std::clamp(selectedClass_, 0, CLASS_COUNT - 1);
    if (ci != 0) {
        return;
    }
    const AbilityDef &def = UNDEAD_HUNTER_ABILITIES.abilities[static_cast<size_t>(abilityIndex)];
    if (abilityCdRem_[static_cast<size_t>(abilityIndex)] > 0.001F) {
        return;
    }
    auto &mana = registry_.get<ecs::Mana>(player_);
    if (mana.current + 1.0e-4F < def.manaCost) {
        noManaFlashTimer_ = 1.2F;
        return;
    }
    mana.current -= def.manaCost;
    abilityCdRem_[static_cast<size_t>(abilityIndex)] = def.cooldown;

    if (abilityIndex == 0) {
        registry_.emplace_or_replace<ecs::LeadFeverEffect>(
            player_, ecs::LeadFeverEffect{config::LEAD_FEVER_DURATION, 0.0F});
        const std::string icon = def.iconPath != nullptr ? std::string(def.iconPath) : std::string{};
        pushStatusHud(StatusHudKind::LeadFever, icon, {140, 220, 160, 255});
        return;
    }
    if (abilityIndex == 1) {
        const Vector2 forward = playerAimDirectionWorld();
        spawnSnareProjectile(forward);
        const float len = std::sqrt(forward.x * forward.x + forward.y * forward.y);
        const Vector2 f = len > 0.001F ? Vector2{forward.x / len, forward.y / len}
                                       : Vector2{1.0F, 0.0F};
        registry_.emplace_or_replace<ecs::SnareDashState>(
            player_, ecs::SnareDashState{config::SNARE_DASH_SPEED, config::SNARE_DASH_DISTANCE, 0.0F,
                                         {-f.x, -f.y}});
        return;
    }
    const Vector2 aimDir = playerAimDirectionWorld();
    registry_.emplace_or_replace<ecs::SlugAimState>(
        player_, ecs::SlugAimState{config::SLUG_AIM_DURATION, 0.0F, aimDir});
}

void GameplayScene::drawHud(ResourceManager &resources) {
    const Font &font = resources.uiFont();
    const int w = config::WINDOW_WIDTH;
    const int h = config::WINDOW_HEIGHT;

    if (!registry_.valid(player_) || !registry_.all_of<ecs::Health, ecs::Mana>(player_)) {
        return;
    }
    syncStatusHudOrder();
    const auto &hp = registry_.get<ecs::Health>(player_);
    const auto &mp = registry_.get<ecs::Mana>(player_);

    const float margin = 16.0F;
    const float portraitR = 58.0F;
    const float barW = 320.0F;
    const float barHpH = 34.0F;
    const float barMpH = 22.0F;
    const float barGap = 4.0F;
    const float barX = margin + portraitR * 2.0F + 12.0F;
    const float hudBottomAll = static_cast<float>(h) - margin;
    const float consumableRowH = 44.0F;
    const float hudBottom = hudBottomAll - consumableRowH;
    const float hpBarTop = hudBottom - barMpH - barGap - barHpH;
    const float icon = 28.0F;
    const int statusIconCount = static_cast<int>(statusHudOrder_.size());
    const float iconLift = statusIconCount > 0 ? icon + 10.0F : 0.0F;

    const float hudPad = 8.0F;
    const float hudLeft = margin;
    const float hudTop = hpBarTop - hudPad - iconLift;
    const float hudW = barX + barW + hudPad - hudLeft;
    const float hudH = hudBottomAll + hudPad - hudTop;
    DrawRectangle(static_cast<int>(hudLeft - 4.0F), static_cast<int>(hudTop - 4.0F),
                  static_cast<int>(hudW + 8.0F), static_cast<int>(hudH + 8.0F), ui::theme::HUD_BACKING);

    const float cx = margin + portraitR;
    const float barsTop = hpBarTop;
    const float barsBottom = hpBarTop + barHpH + barGap + barMpH;
    const float cy = barsTop + (barsBottom - barsTop) * 0.5F;
    DrawCircle(static_cast<int>(cx), static_cast<int>(cy), portraitR, ui::theme::PORTRAIT_FILL);
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), portraitR, ui::theme::PORTRAIT_RING);

    const int ci = std::clamp(selectedClass_, 0, CLASS_COUNT - 1);
    const auto &cls = AVAILABLE_CLASSES[static_cast<size_t>(ci)];
    const char initial = cls.name[0];
    const float portraitFont = 36.0F;
    char oneChar[2] = {initial, '\0'};
    const Vector2 idim = MeasureTextEx(font, oneChar, portraitFont, 1.0F);
    DrawTextEx(font, oneChar, {cx - idim.x * 0.5F, cy - idim.y * 0.5F}, portraitFont, 1.0F,
               RAYWHITE);

    DrawRectangle(static_cast<int>(barX), static_cast<int>(hpBarTop), static_cast<int>(barW),
                  static_cast<int>(barHpH), {50, 20, 20, 255});
    const float hpRatio = hp.max > 0.0F ? hp.current / hp.max : 0.0F;
    DrawRectangle(static_cast<int>(barX), static_cast<int>(hpBarTop),
                  static_cast<int>(barW * hpRatio), static_cast<int>(barHpH), {220, 70, 70, 255});
    DrawRectangleLines(static_cast<int>(barX), static_cast<int>(hpBarTop), static_cast<int>(barW),
                       static_cast<int>(barHpH), {120, 60, 60, 255});

    char hpBuf[64];
    snprintf(hpBuf, sizeof(hpBuf), "%.0f / %.0f", static_cast<double>(hp.current),
             static_cast<double>(hp.max));
    const float hpTextSize = 16.0F;
    const Vector2 hpDim = MeasureTextEx(font, hpBuf, hpTextSize, 1.0F);
    DrawTextEx(font, hpBuf,
               {barX + (barW - hpDim.x) * 0.5F, hpBarTop + (barHpH - hpDim.y) * 0.5F}, hpTextSize,
               1.0F, RAYWHITE);

    // Passive regen indicator (class + equipment); hidden during ManicEffect.
    {
        const float eqRg = inventory_.totalEquippedHpRegenBonus();
        const float totalRg = cls.hpRegen + eqRg;
        if (totalRg > 0.001F && !registry_.all_of<ecs::ManicEffect>(player_)) {
            char regenBuf[32];
            std::snprintf(regenBuf, sizeof(regenBuf), "+%.1f", static_cast<double>(totalRg));
            const float regenSz = 13.0F;
            const Vector2 regenDim = MeasureTextEx(font, regenBuf, regenSz, 1.0F);
            DrawTextEx(font, regenBuf,
                       {barX + barW - regenDim.x - 4.0F,
                        hpBarTop + (barHpH - regenDim.y) * 0.5F},
                       regenSz, 1.0F, ui::theme::LABEL_TEXT);
        }
    }

    const float mpBarTop = hpBarTop + barHpH + barGap;
    DrawRectangle(static_cast<int>(barX), static_cast<int>(mpBarTop), static_cast<int>(barW),
                  static_cast<int>(barMpH), {20, 30, 60, 255});
    const float mpRatio = mp.max > 0.0F ? mp.current / mp.max : 0.0F;
    DrawRectangle(static_cast<int>(barX), static_cast<int>(mpBarTop),
                  static_cast<int>(barW * mpRatio), static_cast<int>(barMpH), {80, 140, 255, 255});
    DrawRectangleLines(static_cast<int>(barX), static_cast<int>(mpBarTop), static_cast<int>(barW),
                       static_cast<int>(barMpH), {60, 90, 160, 255});

    char mpBuf[64];
    snprintf(mpBuf, sizeof(mpBuf), "%.0f / %.0f", static_cast<double>(mp.current),
             static_cast<double>(mp.max));
    const float mpTextSize = 13.0F;
    const Vector2 mpDim = MeasureTextEx(font, mpBuf, mpTextSize, 1.0F);
    DrawTextEx(font, mpBuf,
               {barX + (barW - mpDim.x) * 0.5F, mpBarTop + (barMpH - mpDim.y) * 0.5F}, mpTextSize,
               1.0F, RAYWHITE);

    if (cls.manaRegen > 0.001F) {
        char regenBuf[32];
        std::snprintf(regenBuf, sizeof(regenBuf), "+%.1f",
                      static_cast<double>(cls.manaRegen));
        const float regenSz = 13.0F;
        const Vector2 regenDim = MeasureTextEx(font, regenBuf, regenSz, 1.0F);
        DrawTextEx(font, regenBuf,
                   {barX + barW - regenDim.x - 4.0F,
                    mpBarTop + (barMpH - regenDim.y) * 0.5F},
                   regenSz, 1.0F, ui::theme::LABEL_TEXT);
    }

    // Equipped consumable slots (C / V).
    const float slotY = hudBottom + 8.0F;
    const float slotH = 26.0F;
    const float slotGap = 10.0F;
    const float slotW = (barW - slotGap) * 0.5F;
    const char *keys[static_cast<int>(dreadcast::CONSUMABLE_SLOT_COUNT)] = {"C", "V"};
    for (int si = 0; si < dreadcast::CONSUMABLE_SLOT_COUNT; ++si) {
        const float sx = barX + si * (slotW + slotGap);
        const Rectangle slotR{sx, slotY, slotW, slotH};
        DrawRectangleRec(slotR, ui::theme::SLOT_FILL);
        DrawRectangleLinesEx(slotR, 1.5F, ui::theme::SLOT_BORDER);

        const int idx = inventory_.consumableSlots[static_cast<size_t>(si)];
        DrawTextEx(font, keys[si], {sx + 6.0F, slotY + 4.0F}, 18.0F, 1.0F, RAYWHITE);

        if (idx >= 0 && idx < static_cast<int>(inventory_.items.size())) {
            const auto &it = inventory_.items[static_cast<size_t>(idx)];
            const int count = it.stackCount;
            if (!it.iconPath.empty()) {
                const Texture2D tex = resources.getTexture(it.iconPath);
                if (tex.id != 0) {
                    const Rectangle src{0.0F, 0.0F, static_cast<float>(tex.width),
                                        static_cast<float>(tex.height)};
                    const float keyPad = 26.0F;
                    const float ip = 2.0F;
                    const float maxW = slotW - keyPad - ip * 2.0F;
                    const float maxH = slotH - ip * 2.0F;
                    float dw = maxH * 7.0F / 5.0F;
                    float dh = maxH;
                    if (dw > maxW) {
                        dw = maxW;
                        dh = dw * 5.0F / 7.0F;
                    }
                    const float ix = sx + keyPad + (maxW - dw) * 0.5F;
                    const float iy = slotY + ip + (maxH - dh) * 0.5F;
                    const Rectangle dst{ix, iy, dw, dh};
                    DrawTexturePro(tex, src, dst, {0.0F, 0.0F}, 0.0F, WHITE);
                    if (it.name == "Vial of Cordial Manic" &&
                        hpRatio < config::MANIC_MIN_HP_FRACTION - 1.0e-4F) {
                        const float m = 5.0F;
                        DrawLineEx({ix + m, iy + m}, {ix + dw - m, iy + dh - m}, 3.0F,
                                   Fade(RED, 210));
                        DrawLineEx({ix + m, iy + dh - m}, {ix + dw - m, iy + m}, 3.0F,
                                   Fade(RED, 210));
                    }
                }
            }

            char cntBuf[16];
            std::snprintf(cntBuf, sizeof(cntBuf), "%d", count);
            const Vector2 cDim = MeasureTextEx(font, cntBuf, 14.0F, 1.0F);
            DrawTextEx(font, cntBuf, {sx + slotW - cDim.x - 6.0F, slotY + 6.0F}, 14.0F, 1.0F,
                       RAYWHITE);
        }
    }

    auto drawStatusTimerRingOnly = [](float ix, float iy, float iconSz, float tRem, Color ringCol) {
        if (tRem <= 0.001F) {
            return;
        }
        const float x0 = ix - 2.5F;
        const float y0 = iy - 2.5F;
        const float x1 = ix + iconSz + 2.5F;
        const float y1 = iy + iconSz + 2.5F;
        const float perimeter = (x1 - x0) * 2.0F + (y1 - y0) * 2.0F;
        float skipDist = (1.0F - tRem) * perimeter;
        float drawBudget = tRem * perimeter;
        const float thickness = 3.5F;
        const Color col = Fade(ringCol, 0.85F);

        auto processSeg = [&](Vector2 a, Vector2 b) {
            Vector2 d{b.x - a.x, b.y - a.y};
            float len = std::sqrt(d.x * d.x + d.y * d.y);
            if (len <= 0.001F) {
                return;
            }
            if (skipDist > 0.0F) {
                if (skipDist >= len) {
                    skipDist -= len;
                    return;
                }
                const float u = skipDist / len;
                a = {a.x + d.x * u, a.y + d.y * u};
                d = {b.x - a.x, b.y - a.y};
                len = std::sqrt(d.x * d.x + d.y * d.y);
                skipDist = 0.0F;
                if (len <= 0.001F) {
                    return;
                }
            }
            if (drawBudget <= 0.0F) {
                return;
            }
            if (drawBudget >= len) {
                DrawLineEx(a, b, thickness, col);
                drawBudget -= len;
                return;
            }
            const float tPart = drawBudget / len;
            DrawLineEx(a, {a.x + d.x * tPart, a.y + d.y * tPart}, thickness, col);
            drawBudget = 0.0F;
        };

        const Vector2 topMid{(x0 + x1) * 0.5F, y0};
        processSeg(topMid, {x1, y0});
        processSeg({x1, y0}, {x1, y1});
        processSeg({x1, y1}, {x0, y1});
        processSeg({x0, y1}, {x0, y0});
        processSeg({x0, y0}, topMid);
    };

    const float statusY = hpBarTop - icon - 8.0F;
    const size_t nHud = statusHudOrder_.size();
    for (size_t j = 0; j < nHud; ++j) {
        const ActiveStatusHud &hud = statusHudOrder_[nHud - 1 - j];
        const float ix = barX + static_cast<float>(j) * (icon + 6.0F);
        const float iy = statusY;
        float tRem = 1.0F;
        Color ringCol{255, 220, 180, 255};
        Color fillFallback{60, 50, 55, 255};

        if (hud.kind == StatusHudKind::HealOverTime && registry_.all_of<ecs::HealOverTime>(player_)) {
            const auto &hot = registry_.get<ecs::HealOverTime>(player_);
            tRem = hot.duration > 0.001F ? std::max(0.0F, 1.0F - hot.elapsed / hot.duration) : 0.0F;
            ringCol = {255, 200, 120, 255};
            fillFallback = {120, 20, 30, 255};
        } else if (hud.kind == StatusHudKind::ManicEffect &&
                   registry_.all_of<ecs::ManicEffect>(player_)) {
            const auto &me = registry_.get<ecs::ManicEffect>(player_);
            tRem = me.duration > 0.001F ? std::max(0.0F, 1.0F - me.elapsed / me.duration) : 0.0F;
            ringCol = {255, 245, 180, 255};
            fillFallback = {160, 110, 40, 255};
        } else if (hud.kind == StatusHudKind::LeadFever &&
                   registry_.all_of<ecs::LeadFeverEffect>(player_)) {
            const auto &lf = registry_.get<ecs::LeadFeverEffect>(player_);
            tRem = lf.duration > 0.001F ? std::max(0.0F, 1.0F - lf.elapsed / lf.duration) : 0.0F;
            ringCol = {180, 255, 200, 255};
            fillFallback = {30, 80, 50, 255};
        }

        if (!hud.iconPath.empty()) {
            const Texture2D tex = resources.getTexture(hud.iconPath);
            if (tex.id != 0 && tex.width > 0 && tex.height > 0) {
                const float tw = static_cast<float>(tex.width);
                const float th = static_cast<float>(tex.height);
                const float side = std::min(tw, th);
                const Rectangle src{(tw - side) * 0.5F, (th - side) * 0.5F, side, side};
                const Rectangle dst{ix, iy, icon, icon};
                DrawTexturePro(tex, src, dst, {0.0F, 0.0F}, 0.0F, WHITE);
            } else {
                DrawRectangle(static_cast<int>(ix), static_cast<int>(iy), static_cast<int>(icon),
                              static_cast<int>(icon), fillFallback);
            }
        } else {
            DrawRectangle(static_cast<int>(ix), static_cast<int>(iy), static_cast<int>(icon),
                          static_cast<int>(icon), fillFallback);
        }
        DrawRectangleLines(static_cast<int>(ix), static_cast<int>(iy), static_cast<int>(icon),
                           static_cast<int>(icon), hud.outline);
        drawStatusTimerRingOnly(ix, iy, icon, tRem, ringCol);
        if (hud.kind == StatusHudKind::HealOverTime && hotRefreshFlashTimer_ > 0.001F) {
            const float flashA = std::min(1.0F, hotRefreshFlashTimer_ * 5.0F);
            DrawRectangle(static_cast<int>(ix), static_cast<int>(iy), static_cast<int>(icon),
                          static_cast<int>(icon), Fade(WHITE, 0.4F * flashA));
        }
    }

    // Ability bar (lower-right): Undead Hunter only for now.
    if (ci == 0) {
        constexpr float abSize = 52.0F;
        constexpr float abGap = 8.0F;
        const float abRowW = 3.0F * abSize + 2.0F * abGap;
        const float abRight = static_cast<float>(w) - margin;
        const float abX0 = abRight - abRowW;
        const float abY = hudBottom + 8.0F;
        const float abHudTop = abY - 6.0F;
        const float abHudH = abSize + consumableRowH * 0.5F + 20.0F;
        DrawRectangle(static_cast<int>(abX0 - 6.0F), static_cast<int>(abHudTop),
                      static_cast<int>(abRowW + 12.0F), static_cast<int>(abHudH), ui::theme::HUD_BACKING);

        const Vector2 mouse = GetMousePosition();
        const char *keyLbl[3] = {"1", "2", "3"};
        for (int ai = 0; ai < 3; ++ai) {
            const float ax = abX0 + static_cast<float>(ai) * (abSize + abGap);
            const Rectangle abR{ax, abY, abSize, abSize};
            const AbilityDef &ad = UNDEAD_HUNTER_ABILITIES.abilities[static_cast<size_t>(ai)];
            DrawRectangleRec(abR, ui::theme::SLOT_FILL);
            DrawRectangleLinesEx(abR, 2.0F, ui::theme::SLOT_BORDER);
            if (ad.iconPath != nullptr && ad.iconPath[0] != '\0') {
                const Texture2D at = resources.getTexture(ad.iconPath);
                if (at.id != 0) {
                    const Rectangle src{0.0F, 0.0F, static_cast<float>(at.width),
                                        static_cast<float>(at.height)};
                    DrawTexturePro(at, src, abR, {0.0F, 0.0F}, 0.0F, WHITE);
                } else {
                    const Color ph{static_cast<unsigned char>(70 + ai * 40), 90, 120, 255};
                    DrawRectangleRec(abR, ph);
                }
            } else {
                const Color ph{static_cast<unsigned char>(70 + ai * 35),
                               static_cast<unsigned char>(100 + ai * 20), 130, 255};
                DrawRectangleRec(abR, ph);
            }
            DrawTextEx(font, keyLbl[ai], {ax + 4.0F, abY + 2.0F}, 13.0F, 1.0F, RAYWHITE);

            const float cdTot = ad.cooldown;
            const float cdRem = abilityCdRem_[static_cast<size_t>(ai)];
            if (cdTot > 0.001F && cdRem > 0.001F) {
                const float ratio = std::min(1.0F, cdRem / cdTot);
                DrawRectangleRec(abR, Fade(BLACK, 0.45F * ratio));
                char cdBuf[16];
                std::snprintf(cdBuf, sizeof(cdBuf), "%ds",
                              static_cast<int>(std::ceil(static_cast<double>(cdRem))));
                const float cdFs = 18.0F;
                const Vector2 cdDim = MeasureTextEx(font, cdBuf, cdFs, 1.0F);
                DrawTextEx(font, cdBuf,
                           {abR.x + (abR.width - cdDim.x) * 0.5F + 1.0F,
                            abR.y + (abR.height - cdDim.y) * 0.5F + 1.0F},
                           cdFs, 1.0F, Fade(BLACK, 180));
                DrawTextEx(font, cdBuf,
                           {abR.x + (abR.width - cdDim.x) * 0.5F,
                            abR.y + (abR.height - cdDim.y) * 0.5F},
                           cdFs, 1.0F, {200, 220, 255, 255});
            }

            if (CheckCollisionPointRec(mouse, abR)) {
                const std::string title = ad.name;
                const std::string body = ad.description;
                const float tipSz = 13.0F;
                const Vector2 titleDim = MeasureTextEx(font, title.c_str(), tipSz + 2.0F, 1.0F);
                float descH = 0.0F;
                float descW = 0.0F;
                if (!body.empty()) {
                    std::istringstream iss(body);
                    std::string line;
                    while (std::getline(iss, line)) {
                        const Vector2 d = MeasureTextEx(font, line.c_str(), tipSz, 1.0F);
                        descH += d.y + 4.0F;
                        descW = std::max(descW, d.x);
                    }
                    descH = std::max(0.0F, descH - 4.0F);
                }
                const float pad = 8.0F;
                const float gap = 6.0F;
                const float tw = std::max(titleDim.x, descW) + pad * 2.0F;
                const float th = titleDim.y + gap + descH + pad * 2.0F;
                Rectangle tip{mouse.x + 14.0F, mouse.y - th - 14.0F, tw, th};
                clampRectToScreen(tip, w, h);
                DrawRectangleRec(tip, Fade(ui::theme::PANEL_FILL, 235));
                DrawRectangleLinesEx(tip, 1.5F, ui::theme::PANEL_BORDER);
                float yc = tip.y + pad;
                DrawTextEx(font, title.c_str(), {tip.x + pad, yc}, tipSz + 2.0F, 1.0F, RAYWHITE);
                yc += titleDim.y + gap;
                if (!body.empty()) {
                    std::istringstream iss2(body);
                    std::string line2;
                    while (std::getline(iss2, line2)) {
                        DrawTextEx(font, line2.c_str(), {tip.x + pad, yc}, tipSz, 1.0F,
                                   ui::theme::LABEL_TEXT);
                        const Vector2 d = MeasureTextEx(font, line2.c_str(), tipSz, 1.0F);
                        yc += d.y + 4.0F;
                    }
                }
            }
        }
    }
}

void GameplayScene::drawFlashMessages(ResourceManager &resources) {
    const Font &font = resources.uiFont();
    const int w = config::WINDOW_WIDTH;
    const int h = config::WINDOW_HEIGHT;
    const float msgSize = 20.0F;
    if (noManaFlashTimer_ > 0.0F) {
        const char *msg = "Not enough mana";
        const Vector2 dim = MeasureTextEx(font, msg, msgSize, 1.0F);
        const float alpha = std::min(1.0F, noManaFlashTimer_ * 2.0F);
        const Color c = {255, 200, 200, static_cast<unsigned char>(alpha * 255.0F)};
        DrawTextEx(font, msg, {(w - dim.x) * 0.5F, h * 0.5F + 80.0F}, msgSize, 1.0F, c);
    }
    if (inventoryFullFlashTimer_ > 0.0F) {
        const char *msg = "Inventory full";
        const Vector2 dim = MeasureTextEx(font, msg, msgSize, 1.0F);
        const float alpha = std::min(1.0F, inventoryFullFlashTimer_ * 2.0F);
        const Color c = {255, 220, 160, static_cast<unsigned char>(alpha * 255.0F)};
        DrawTextEx(font, msg, {(w - dim.x) * 0.5F, h * 0.5F + 110.0F}, msgSize, 1.0F, c);
    }
}

void GameplayScene::drawLootPickupHighlight([[maybe_unused]] ResourceManager &resources) {
    if (inventoryUi_.isOpen() || paused_ || gameOver_) {
        return;
    }
    if (!registry_.valid(hoveredPickup_) || !registry_.all_of<ecs::ItemPickup, ecs::Transform,
                                                                  ecs::Sprite>(hoveredPickup_)) {
        return;
    }
    const auto &t = registry_.get<ecs::Transform>(hoveredPickup_);
    const auto &s = registry_.get<ecs::Sprite>(hoveredPickup_);
    const Vector2 iso = worldToIso(t.position);
    const float pulse = 0.5F + 0.5F * std::sinf(static_cast<float>(GetTime()) * 4.0F);
    const float r = s.width * 0.65F + pulse * 8.0F;
    const unsigned char a =
        static_cast<unsigned char>(160.0F + pulse * 95.0F);
    DrawCircleLines(static_cast<int>(iso.x), static_cast<int>(iso.y), r,
                    {220, 100, 60, a});
}

void GameplayScene::drawLootProximityPrompt(ResourceManager &resources) {
    if (inventoryUi_.isOpen() || paused_ || gameOver_) {
        return;
    }
    const Font &font = resources.uiFont();

    if (registry_.valid(hoveredInteract_) && registry_.all_of<ecs::Interactable, ecs::Transform>(
                                                  hoveredInteract_)) {
        const auto &inter = registry_.get<ecs::Interactable>(hoveredInteract_);
        if (!inter.opened) {
            const auto &pickupT = registry_.get<ecs::Transform>(hoveredInteract_);
            const float nameSz = 16.0F;
            const float promptSz = 18.0F;
            const char *prompt = "Press [F] to open";
            const Vector2 iso = worldToIso(pickupT.position);
            const Vector2 screen = GetWorldToScreen2D(iso, camera_.camera());
            const Vector2 nameDim = MeasureTextEx(font, inter.name.c_str(), nameSz, 1.0F);
            const Vector2 prDim = MeasureTextEx(font, prompt, promptSz, 1.0F);
            const float boxW = std::max(nameDim.x, prDim.x) + 24.0F;
            const float boxH = nameDim.y + prDim.y + 28.0F;
            const float bx = screen.x - boxW * 0.5F;
            const float by = screen.y - 52.0F;
            const Rectangle bg{bx, by, boxW, boxH};
            DrawRectangleRec(bg, {0, 0, 0, 210});
            DrawRectangleLinesEx(bg, 2.0F, ui::theme::BTN_BORDER);
            DrawTextEx(font, inter.name.c_str(), {bx + 12.0F, by + 8.0F}, nameSz, 1.0F, RAYWHITE);
            DrawTextEx(font, prompt, {bx + (boxW - prDim.x) * 0.5F, by + 16.0F + nameDim.y},
                       promptSz, 1.0F, ui::theme::LABEL_TEXT);
        }
    }

    if (!registry_.valid(hoveredPickup_) || !registry_.all_of<ecs::ItemPickup>(hoveredPickup_)) {
        return;
    }
    const int idx = registry_.get<ecs::ItemPickup>(hoveredPickup_).itemIndex;
    if (idx < 0 || idx >= static_cast<int>(inventory_.items.size())) {
        return;
    }
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Transform>(player_)) {
        return;
    }
    const char *name = inventory_.items[static_cast<size_t>(idx)].name.c_str();
    const float nameSz = 16.0F;
    const char *prompt = "Press [E] to pick up";
    const float promptSz = 18.0F;
    const Vector2 mouse = GetMousePosition();
    const Vector2 nameDim = MeasureTextEx(font, name, nameSz, 1.0F);
    const Vector2 prDim = MeasureTextEx(font, prompt, promptSz, 1.0F);
    const bool altHeld = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    const std::string &desc = inventory_.items[static_cast<size_t>(idx)].description;
    float boxW = std::max(nameDim.x, prDim.x) + 24.0F;
    float boxH = nameDim.y + prDim.y + 28.0F;
    Vector2 descDim{};
    if (altHeld && !desc.empty()) {
        const float descSz = 14.0F;
        descDim = MeasureTextEx(font, desc.c_str(), descSz, 1.0F);
        boxW = std::max(boxW, descDim.x + 24.0F);
        boxH += descDim.y + 8.0F;
    }
    Rectangle bg{mouse.x + 14.0F, mouse.y - boxH - 14.0F, boxW, boxH};
    clampRectToScreen(bg, config::WINDOW_WIDTH, config::WINDOW_HEIGHT);
    DrawRectangleRec(bg, {0, 0, 0, 210});
    DrawRectangleLinesEx(bg, 2.0F, ui::theme::BTN_BORDER);
    DrawTextEx(font, name, {bg.x + 12.0F, bg.y + 8.0F}, nameSz, 1.0F, RAYWHITE);
    float textY = bg.y + 16.0F + nameDim.y;
    if (altHeld && !desc.empty()) {
        const float descSz = 14.0F;
        DrawTextEx(font, desc.c_str(), {bg.x + 12.0F, textY}, descSz, 1.0F, ui::theme::LABEL_TEXT);
        textY += descDim.y + 6.0F;
    }
    DrawTextEx(font, prompt, {bg.x + (boxW - prDim.x) * 0.5F, textY}, promptSz, 1.0F,
               ui::theme::LABEL_TEXT);
}

void GameplayScene::drawGameOverScreen(ResourceManager &resources) {
    const int w = config::WINDOW_WIDTH;
    const int h = config::WINDOW_HEIGHT;
    DrawRectangle(0, 0, w, h, {0, 0, 0, 200});

    const Font &font = resources.uiFont();
    const char *title = "GAME OVER";
    const float ts = 44.0F;
    const Vector2 td = MeasureTextEx(font, title, ts, 1.0F);
    DrawTextEx(font, title, {(w - td.x) * 0.5F, h * 0.5F - 130.0F}, ts, 1.0F, RAYWHITE);

    char killBuf[64];
    snprintf(killBuf, sizeof(killBuf), "Enemies slain: %d", enemiesSlain_);
    const float ks = 22.0F;
    const Vector2 kd = MeasureTextEx(font, killBuf, ks, 1.0F);
    DrawTextEx(font, killBuf, {(w - kd.x) * 0.5F, h * 0.5F - 55.0F}, ks, 1.0F,
               ui::theme::SUBTITLE_TEXT);

    const Vector2 mouse = GetMousePosition();
    retryButton_.draw(font, 24.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                      ui::theme::BTN_BORDER);
    gameOverMenuButton_.draw(font, 24.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER,
                             RAYWHITE, ui::theme::BTN_BORDER);
}

void GameplayScene::drawPauseOverlay(ResourceManager &resources) {
    const int w = config::WINDOW_WIDTH;
    const int h = config::WINDOW_HEIGHT;
    DrawRectangle(0, 0, w, h, ui::theme::PAUSE_OVERLAY);

    const Font &font = resources.uiFont();
    const char *title = "PAUSED";
    const float ts = 40.0F;
    const Vector2 td = MeasureTextEx(font, title, ts, 1.0F);
    DrawTextEx(font, title, {(w - td.x) * 0.5F, h * 0.5F - 170.0F}, ts, 1.0F, RAYWHITE);

    const Vector2 mouse = GetMousePosition();
    resumeButton_.draw(font, 24.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                       ui::theme::BTN_BORDER);
    settingsPauseButton_.draw(font, 24.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER,
                              RAYWHITE, ui::theme::BTN_BORDER);
    mainMenuButton_.draw(font, 24.0F, mouse, ui::theme::BTN_FILL, ui::theme::BTN_HOVER, RAYWHITE,
                         ui::theme::BTN_BORDER);
}

} // namespace dreadcast
