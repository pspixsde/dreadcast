#include "scenes/gameplay_scene.hpp"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

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
#include "game/character.hpp"
#include "game/game_data.hpp"
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

void spawnLavaBlock(entt::registry &reg, float cx, float cy, float halfW, float halfH) {
    const auto e = reg.create();
    reg.emplace<ecs::Transform>(e, ecs::Transform{{cx, cy}, 0.0F});
    reg.emplace<ecs::Lava>(e, ecs::Lava{halfW, halfH});
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

/// Keep ability tooltip from overlapping the lower-right ability/consumable cluster (drawn on top).
void layoutAbilityDescriptionTooltip(Rectangle &tip, const Rectangle &abilityCluster, const Vector2 &mouse,
                                   int screenW, int screenH) {
    tip.x = mouse.x;
    tip.y = mouse.y - tip.height;
    clampRectToScreen(tip, screenW, screenH);
    for (int iter = 0; iter < 6 && CheckCollisionRecs(tip, abilityCluster); ++iter) {
        tip.y = abilityCluster.y - tip.height - 10.0F;
        if (tip.x < 4.0F) {
            tip.x = 4.0F;
        }
        if (tip.x + tip.width > static_cast<float>(screenW) - 4.0F) {
            tip.x = static_cast<float>(screenW) - tip.width - 4.0F;
        }
        if (tip.y < 4.0F) {
            tip.y = 4.0F;
        }
    }
    if (tip.y + tip.height > static_cast<float>(screenH) - 4.0F) {
        tip.y = static_cast<float>(screenH) - tip.height - 4.0F;
    }
}

/// Keybind glyph in a small black square, slightly above/outside the icon corner (screen-space HUD).
void drawHudKeyBadge(const Font &font, const char *keyChr, float iconLeft, float iconTop) {
    constexpr float kSq = 20.0F;
    constexpr float protrude = 4.0F;
    const float bx = iconLeft - protrude * 0.65F;
    const float by = iconTop - protrude * 1.2F;
    DrawRectangle(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(kSq),
                  static_cast<int>(kSq), Color{50, 50, 50, 255});
    DrawRectangleLines(static_cast<int>(bx), static_cast<int>(by), static_cast<int>(kSq),
                       static_cast<int>(kSq), Color{15, 15, 15, 200});
    constexpr float ks = 15.0F;
    const Vector2 kd = MeasureTextEx(font, keyChr, ks, 1.0F);
    DrawTextEx(font, keyChr, {bx + (kSq - kd.x) * 0.5F, by + (kSq - kd.y) * 0.5F}, ks, 1.0F,
               RAYWHITE);
}

[[nodiscard]] Rectangle hudKeyBadgeOuterRect(float iconLeft, float iconTop) {
    constexpr float kSq = 20.0F;
    constexpr float protrude = 4.0F;
    const float bx = iconLeft - protrude * 0.65F;
    const float by = iconTop - protrude * 1.2F;
    return {bx, by, kSq, kSq};
}

[[nodiscard]] float playerVisionRadiusForFog(const entt::registry &reg, entt::entity player) {
    if (reg.valid(player) && reg.all_of<ecs::PlayerMoveStats>(player)) {
        return reg.get<ecs::PlayerMoveStats>(player).visionRange;
    }
    return config::FOG_OF_WAR_RADIUS;
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

void GameplayScene::spawnLavas(const MapData &map) {
    for (const LavaData &w : map.lavas) {
        spawnLavaBlock(registry_, w.cx, w.cy, w.halfW, w.halfH);
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
    lavaDamageAccumulator_ = 0.0F;
    spiritRefreshFlashTimer_ = 0.0F;
    MapData map = defaultMapData();
    MapData loaded;
    if (loaded.loadFromFile("assets/maps/default.map")) {
        map = loaded;
    }
    loadedMap_ = map;
    fullMapOpen_ = false;
    anvilOpen_ = false;
    activeAnvil_ = entt::null;
    anvilTab_ = 0;
    forgeSlots_.fill(-1);
    disassembleInputIndex_ = -1;
    disassembleOutputPool_.fill(-1);
    disassembleOutputCount_ = 0;
    anvilBenchDragKind_ = AnvilBenchDragKind::None;
    anvilBenchForgeSlot_ = -1;
    anvilBenchDisOutSlot_ = -1;
    anvilBenchDragPoolIdx_ = -1;
    hellhoundPrevAgitated_.clear();

    const int classIdx = std::clamp(selectedClass_, 0, std::max(0, characterCount() - 1));
    const CharacterClass &cls = characterAt(classIdx);

    player_ = registry_.create();
    registry_.emplace<ecs::Transform>(player_, ecs::Transform{map.playerSpawn, 0.0F});
    registry_.emplace<ecs::Velocity>(player_, ecs::Velocity{});
    registry_.emplace<ecs::Sprite>(player_,
                                   ecs::Sprite{{0, 220, 255, 255}, 36.0F, 36.0F});
    registry_.emplace<ecs::Health>(player_, ecs::Health{cls.baseMaxHp, cls.baseMaxHp});
    registry_.emplace<ecs::Mana>(player_, ecs::Mana{cls.baseMaxMana, cls.baseMaxMana});
    {
        ecs::MeleeAttacker melee{};
        melee.damage = cls.meleeDamage;
        melee.knockback = config::MELEE_KNOCKBACK;
        melee.range = cls.meleeRange;
        registry_.emplace<ecs::MeleeAttacker>(player_, melee);
    }
    registry_.emplace<ecs::PlayerCombatBase>(player_, ecs::PlayerCombatBase{cls.rangedDamage});
    registry_.emplace<ecs::PlayerMoveStats>(player_,
                                            ecs::PlayerMoveStats{cls.moveSpeed, cls.visionRange});
    registry_.emplace<ecs::PlayerClassStats>(player_,
                                             ecs::PlayerClassStats{cls.baseMaxHp, cls.baseMaxMana});
    registry_.emplace<ecs::PlayerLevel>(
        player_, ecs::PlayerLevel{1,
                                  0.0F,
                                  100.0F,
                                  0.0F,
                                  cls.levelMaxHpGain,
                                  cls.levelMaxManaGain,
                                  cls.levelProjectileDamageGain,
                                  cls.levelMeleeDamageGain});
    registry_.emplace<ecs::Facing>(player_, ecs::Facing{});
    registry_.emplace<ecs::Player>(player_);
    registry_.emplace<ecs::ChamberState>(player_, ecs::ChamberState{});

    spawnWalls(map);
    spawnLavas(map);

    for (const SolidShapeData &sd : map.solidShapes) {
        if (sd.verts.size() < 3) {
            continue;
        }
        const auto poly = registry_.create();
        registry_.emplace<ecs::Transform>(poly, ecs::Transform{{0.0F, 0.0F}, 0.0F});
        registry_.emplace<ecs::SolidPolygon>(poly, ecs::SolidPolygon{sd.verts});
        registry_.emplace<ecs::Sprite>(poly, ecs::Sprite{{48, 44, 38, 255}, 2.0F, 2.0F});
    }
    for (const AnvilData &an : map.anvils) {
        const auto anv = registry_.create();
        registry_.emplace<ecs::Transform>(anv, ecs::Transform{{an.cx, an.cy}, 0.0F});
        registry_.emplace<ecs::Velocity>(anv, ecs::Velocity{});
        registry_.emplace<ecs::Sprite>(anv, ecs::Sprite{{140, 110, 80, 255}, 52.0F, 36.0F});
        registry_.emplace<ecs::Interactable>(anv, ecs::Interactable{"Anvil", false});
    }

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
        registry_.emplace<ecs::EnemyXpReward>(e, ecs::EnemyXpReward{hellhound ? 30.0F : 25.0F});
        registry_.emplace<ecs::EnemyDisplayLevel>(e, ecs::EnemyDisplayLevel{1});
    }

    if (map.hasCasket) {
        const auto casket = registry_.create();
        registry_.emplace<ecs::Transform>(
            casket, ecs::Transform{{map.casket.cx, map.casket.cy}, 0.0F});
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

    prevPlayerHp_ = cls.baseMaxHp;
}

Vector2 GameplayScene::worldMouseFromScreen(const Vector2 &screenMouse) const {
    const Vector2 isoMouse = GetScreenToWorld2D(screenMouse, camera_.camera());
    return isoToWorld(isoMouse);
}

void GameplayScene::applyPlayerMaxManaFromEquipment() {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Mana>(player_)) {
        return;
    }
    auto &mp = registry_.get<ecs::Mana>(player_);
    const float bonus = inventory_.totalEquippedMaxManaBonus();
    const float baseMana = registry_.all_of<ecs::PlayerClassStats>(player_)
                               ? registry_.get<ecs::PlayerClassStats>(player_).baseMaxMana
                               : 100.0F;
    const float newMax = baseMana + bonus;
    if (std::fabs(mp.max - newMax) < 0.001F) {
        return;
    }
    const float mpRatio = mp.max > 0.001F ? mp.current / mp.max : 1.0F;
    mp.max = newMax;
    mp.current = std::clamp(mpRatio * mp.max, 0.0F, mp.max);
}

void GameplayScene::applyPlayerMaxHpFromEquipment() {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Health>(player_)) {
        return;
    }
    auto &hp = registry_.get<ecs::Health>(player_);
    const float bonus = inventory_.totalEquippedMaxHpBonus();
    const float baseHp = registry_.all_of<ecs::PlayerClassStats>(player_)
                             ? registry_.get<ecs::PlayerClassStats>(player_).baseMaxHp
                             : config::PLAYER_BASE_MAX_HP;
    const float newMax = baseHp + bonus;
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
    if (!registry_.all_of<ecs::HealOverTime>(player_)) {
        return;
    }
    auto &hot = registry_.get<ecs::HealOverTime>(player_);
    hot.elapsed += fixedDt;

    // Healing is suppressed while Cordial Manic is active; timer still runs.
    if (!registry_.all_of<ecs::ManicEffect>(player_)) {
        auto &hp = registry_.get<ecs::Health>(player_);
        const float rate = hot.totalHeal / hot.duration;
        float add = rate * fixedDt;
        const float remainingHeal = hot.totalHeal - hot.healedSoFar;
        if (add > remainingHeal) {
            add = remainingHeal;
        }
        hot.healedSoFar += add;
        hp.current = std::min(hp.max, hp.current + add);
    }

    if (hot.elapsed >= hot.duration - 1.0e-4F || hot.healedSoFar >= hot.totalHeal - 1.0e-3F) {
        registry_.remove<ecs::HealOverTime>(player_);
    }
}

void GameplayScene::tickManaRegenOverTime(float fixedDt) {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Mana>(player_)) {
        return;
    }
    if (!registry_.all_of<ecs::ManaRegenOverTime>(player_)) {
        return;
    }
    auto &mrot = registry_.get<ecs::ManaRegenOverTime>(player_);
    mrot.elapsed += fixedDt;
    auto &mp = registry_.get<ecs::Mana>(player_);
    const float rate = mrot.totalMana / mrot.duration;
    float add = rate * fixedDt;
    const float remaining = mrot.totalMana - mrot.regenedSoFar;
    if (add > remaining) {
        add = remaining;
    }
    mrot.regenedSoFar += add;
    mp.current = std::min(mp.max, mp.current + add);
    if (mrot.elapsed >= mrot.duration - 1.0e-4F ||
        mrot.regenedSoFar >= mrot.totalMana - 1.0e-3F) {
        registry_.remove<ecs::ManaRegenOverTime>(player_);
    }
}

void GameplayScene::tickLavaHazard(float fixedDt) {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Health, ecs::Transform>(player_)) {
        return;
    }
    const auto &pt = registry_.get<ecs::Transform>(player_);
    const float px = pt.position.x;
    const float py = pt.position.y;
    bool inLava = false;
    for (const auto e : registry_.view<ecs::Lava, ecs::Transform>()) {
        const auto &t = registry_.get<ecs::Transform>(e);
        const auto &lv = registry_.get<ecs::Lava>(e);
        if (px >= t.position.x - lv.halfW && px <= t.position.x + lv.halfW &&
            py >= t.position.y - lv.halfH && py <= t.position.y + lv.halfH) {
            inLava = true;
            break;
        }
    }
    if (!inLava) {
        lavaDamageAccumulator_ = 0.0F;
        return;
    }
    lavaDamageAccumulator_ += fixedDt;
    while (lavaDamageAccumulator_ >= config::LAVA_DAMAGE_INTERVAL) {
        lavaDamageAccumulator_ -= config::LAVA_DAMAGE_INTERVAL;
        auto &hp = registry_.get<ecs::Health>(player_);
        hp.current -= config::LAVA_DAMAGE_PER_TICK;
        hp.current = std::max(0.0F, hp.current);
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
            const int oldLast = static_cast<int>(inventory_.items.size()) - 1;
            inventory_.removeItemAtIndex(idx);
            ecs::collision::rewrite_ground_pickup_indices_after_remove(registry_, idx, oldLast);
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
            const int oldLast = static_cast<int>(inventory_.items.size()) - 1;
            inventory_.removeItemAtIndex(idx);
            ecs::collision::rewrite_ground_pickup_indices_after_remove(registry_, idx, oldLast);
        }
        return;
    }
    if (it.name == "Vial of Raw Spirit") {
        const bool wasActive =
            registry_.valid(player_) && registry_.all_of<ecs::ManaRegenOverTime>(player_);
        it.stackCount -= 1;
        if (it.stackCount <= 0) {
            inventory_.consumableSlots[static_cast<size_t>(slotIndex)] = -1;
            const int oldLast = static_cast<int>(inventory_.items.size()) - 1;
            inventory_.removeItemAtIndex(idx);
            ecs::collision::rewrite_ground_pickup_indices_after_remove(registry_, idx, oldLast);
        }
        applyVialRawSpiritMana(wasActive);
        return;
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
            const int oldLast = static_cast<int>(inventory_.items.size()) - 1;
            inventory_.removeItemAtIndex(idx);
            ecs::collision::rewrite_ground_pickup_indices_after_remove(registry_, idx, oldLast);
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
            const int oldLast = static_cast<int>(inventory_.items.size()) - 1;
            inventory_.removeItemAtIndex(idx);
            ecs::collision::rewrite_ground_pickup_indices_after_remove(registry_, idx, oldLast);
        }
        return;
    }
    if (it.name == "Vial of Raw Spirit") {
        const bool wasActive =
            registry_.valid(player_) && registry_.all_of<ecs::ManaRegenOverTime>(player_);
        it.stackCount -= 1;
        if (it.stackCount <= 0) {
            inventory_.bagSlots[static_cast<size_t>(bagSlot)] = -1;
            const int oldLast = static_cast<int>(inventory_.items.size()) - 1;
            inventory_.removeItemAtIndex(idx);
            ecs::collision::rewrite_ground_pickup_indices_after_remove(registry_, idx, oldLast);
        }
        applyVialRawSpiritMana(wasActive);
        return;
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

void GameplayScene::applyVialRawSpiritMana(bool wasAlreadyActive) {
    if (!registry_.valid(player_)) {
        return;
    }
    if (wasAlreadyActive) {
        spiritRefreshFlashTimer_ = 0.35F;
    }
    registry_.emplace_or_replace<ecs::ManaRegenOverTime>(
        player_, ecs::ManaRegenOverTime{config::RAW_SPIRIT_MANA_TOTAL, config::RAW_SPIRIT_DURATION,
                                        0.0F, 0.0F});
    pushStatusHud(StatusHudKind::ManaRegenOverTime, "assets/textures/items/vial_raw_spirit_icon.png",
                  {120, 170, 255, 255});
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
    if (hp.max < 0.001F) {
        return;
    }
    const float thresh = config::RUNIC_SHELL_HP_THRESHOLD * hp.max;
    // Only trigger when HP crosses from above the threshold to at-or-below this tick (damage),
    // not when already low (cooldown expiry, equip while hurt, etc.).
    float hpAtTickStart = hp.current;
    const auto hpIt = hpBeforeFixedStep_.find(player_);
    if (hpIt != hpBeforeFixedStep_.end()) {
        hpAtTickStart = hpIt->second;
    }
    if (!(hpAtTickStart > thresh + 1.0e-4F && hp.current <= thresh + 1.0e-4F)) {
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
    spiritRefreshFlashTimer_ = std::max(0.0F, spiritRefreshFlashTimer_ - frameDt);
    runicShellFlashTimer_ = std::max(0.0F, runicShellFlashTimer_ - frameDt);
    snareImpactFlash_ = std::max(0.0F, snareImpactFlash_ - frameDt);
    hurtGruntCooldown_ = std::max(0.0F, hurtGruntCooldown_ - frameDt);

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
                if (anvilOpen_) {
                    resetAnvilWorkbench();
                    anvilOpen_ = false;
                    activeAnvil_ = entt::null;
                    inventoryUi_.setPanelLayoutShift(0.0F);
                }
            }
        }
    }

    inventoryUi_.setPanelLayoutShift(anvilOpen_ ? 400.0F : 0.0F);

    if (inventoryUi_.isOpen()) {
        if (input.isKeyPressed(KEY_ESCAPE)) {
            if (anvilOpen_) {
                resetAnvilWorkbench();
                anvilOpen_ = false;
                activeAnvil_ = entt::null;
                inventoryUi_.setPanelLayoutShift(0.0F);
            }
            inventoryUi_.setOpen(false);
            aimScreenPos_ = input.mousePosition();
        } else {
            const ui::AnvilUiLayout anvilLayout =
                anvilOpen_ ? buildAnvilUiLayout() : ui::AnvilUiLayout{};
            if (anvilOpen_) {
                tickAnvilUi(input, resources, anvilLayout);
            }
            const ui::InventoryAction invAction = inventoryUi_.update(
                input, inventory_, resources.settings().separateDropsWhenFull,
                anvilOpen_ ? &anvilLayout : nullptr);
            if (invAction.type == ui::InventoryAction::AnvilForgePlace ||
                invAction.type == ui::InventoryAction::AnvilDisassembleInputPlace) {
                handleInventoryAnvilAction(invAction);
            } else if (invAction.type == ui::InventoryAction::Drop) {
                spawnItemPickupAtPlayer(invAction.itemIndex);
            } else if (invAction.type == ui::InventoryAction::SeparateDropWorld) {
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
    applyPlayerMaxManaFromEquipment();

    bool aimSnappedAfterPauseEsc = false;
    if (!inventoryUi_.isOpen() && input.isKeyPressed(KEY_ESCAPE)) {
        if (paused_) {
            paused_ = false;
            aimScreenPos_ = input.mousePosition();
            aimScreenInit_ = true;
            aimSnappedAfterPauseEsc = true;
        } else {
            paused_ = true;
        }
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
            aimScreenPos_ = mouse;
            aimScreenInit_ = true;
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
    if (!paused_ && !inventoryUi_.isOpen() && !fullMapOpen_ && !aimSnappedAfterPauseEsc) {
        const Vector2 d = input.mouseDelta();
        aimScreenPos_.x = std::clamp(aimScreenPos_.x + d.x * aimMouseSensitivity_, 0.0F,
                                     static_cast<float>(config::WINDOW_WIDTH));
        aimScreenPos_.y = std::clamp(aimScreenPos_.y + d.y * aimMouseSensitivity_, 0.0F,
                                     static_cast<float>(config::WINDOW_HEIGHT));
    }

    const bool invOpen = inventoryUi_.isOpen();
    const bool worldBlocked = invOpen || fullMapOpen_;

    if (!paused_ && !invOpen && input.isKeyPressed(KEY_M)) {
        fullMapOpen_ = !fullMapOpen_;
    }
    if (fullMapOpen_ && input.isKeyPressed(KEY_ESCAPE)) {
        fullMapOpen_ = false;
    }

    if (!worldBlocked) {
        ecs::input_system(registry_, input, camera_.camera(), aimScreenPos_);
        if (ecs::combat_player_ranged(registry_, input, camera_.camera(), player_, noManaFlashTimer_,
                                      aimScreenPos_)) {
            if (!registry_.all_of<ecs::LeadFeverEffect>(player_)) {
                Sound gun = resources.getSound("assets/sounds/gun_shot.wav");
                if (IsSoundValid(gun)) {
                    const float p = 0.85F + static_cast<float>(GetRandomValue(0, 30)) / 100.0F;
                    SetSoundPitch(gun, p);
                    PlaySound(gun);
                }
            }
        }
    } else if (registry_.valid(player_) && registry_.all_of<ecs::Velocity>(player_)) {
        auto &vel = registry_.get<ecs::Velocity>(player_);
        vel.value.x = 0.0F;
        vel.value.y = 0.0F;
    }

    const int steps = fixedTimer_.consumeSteps(frameDt);
    for (int i = 0; i < steps; ++i) {
        hpBeforeFixedStep_.clear();
        for (const auto e : registry_.view<ecs::Health>()) {
            hpBeforeFixedStep_[e] = registry_.get<ecs::Health>(e).current;
        }

        tickStunnedEnemies(config::FIXED_DT);
        tickAbilityCooldowns(config::FIXED_DT);
        tickLeadFeverEffect(config::FIXED_DT);
        tickSnareDash(config::FIXED_DT);
        tickSlugAim(config::FIXED_DT, resources);
        tickChamberState(config::FIXED_DT);
        if (!worldBlocked) {
            ecs::combat_player_melee(registry_, input, player_, config::FIXED_DT);
        } else if (registry_.valid(player_) && registry_.all_of<ecs::MeleeAttacker>(player_)) {
            auto &melee = registry_.get<ecs::MeleeAttacker>(player_);
            melee.phase = ecs::MeleeAttacker::Phase::Idle;
            melee.phaseTimer = 0.0F;
            melee.swingIndex = 0;
            melee.hitAppliedThisSwing = false;
        }
        ecs::enemy_ai_system(registry_, config::FIXED_DT, player_, &inventory_);

        for (const auto e : registry_.view<ecs::Enemy, ecs::EnemyAI, ecs::Agitation>()) {
            if (registry_.get<ecs::EnemyAI>(e).type != ecs::EnemyType::Hellhound) {
                continue;
            }
            const bool now = registry_.get<ecs::Agitation>(e).agitated;
            bool &prev = hellhoundPrevAgitated_[e];
            if (now && !prev) {
                Sound ag = resources.getSound("assets/sounds/hellhound_agro.wav");
                if (IsSoundValid(ag)) {
                    const float p = 0.85F + static_cast<float>(GetRandomValue(0, 30)) / 100.0F;
                    SetSoundPitch(ag, p);
                    PlaySound(ag);
                }
            }
            prev = now;
        }

        ecs::movement_system(registry_, config::FIXED_DT);
        ecs::wall_resolve_collisions(registry_);
        ecs::unit_resolve_collisions(registry_);
        tickLavaHazard(config::FIXED_DT);
        ecs::wall_destroy_projectiles(registry_);
        ecs::projectile_system(registry_, config::FIXED_DT);
        ecs::collision::projectile_hits(registry_, player_, &inventory_, &snareImpactWorld_,
                                        &snareImpactFlash_);
        ecs::collision::player_pickup_mana_shards(registry_, player_);
        ecs::death_system(registry_, player_, &enemiesSlain_);
        tickManicEffect(config::FIXED_DT);
        tickHealOverTime(config::FIXED_DT);
        tickManaRegenOverTime(config::FIXED_DT);
        tickRunicShellCooldown(config::FIXED_DT);
        checkRunicShellTrigger();

        // Passive regen based on the selected class (+ equipment); blocked during ManicEffect.
        if (registry_.valid(player_) && registry_.all_of<ecs::Health, ecs::Mana>(player_) &&
            !registry_.all_of<ecs::ManicEffect>(player_)) {
            const int ci = std::clamp(selectedClass_, 0, std::max(0, characterCount() - 1));
            const auto &cls = characterAt(ci);
            auto &hp2 = registry_.get<ecs::Health>(player_);
            auto &mp = registry_.get<ecs::Mana>(player_);
            hp2.current = std::min(
                hp2.max, hp2.current + cls.hpRegen * config::FIXED_DT +
                             inventory_.totalEquippedHpRegenBonus() * config::FIXED_DT);
            mp.current = std::min(mp.max, mp.current + cls.manaRegen * config::FIXED_DT);
        } else if (registry_.valid(player_) && registry_.all_of<ecs::Mana>(player_) &&
                   registry_.all_of<ecs::ManicEffect>(player_)) {
            auto &mp = registry_.get<ecs::Mana>(player_);
            const int ci = std::clamp(selectedClass_, 0, std::max(0, characterCount() - 1));
            const auto &cls = characterAt(ci);
            mp.current = std::min(mp.max, mp.current + cls.manaRegen * config::FIXED_DT);
        }

        if (resources.settings().showDamageNumbers) {
            static int floatDriftCounter = 0;
            for (const auto e : registry_.view<ecs::Health, ecs::Transform>()) {
                const auto prevIt = hpBeforeFixedStep_.find(e);
                if (prevIt == hpBeforeFixedStep_.end()) {
                    continue;
                }
                const float oldH = prevIt->second;
                const float newH = registry_.get<ecs::Health>(e).current;
                const float diff = newH - oldH;
                if (diff <= -0.25F) {
                    FloatingNumber fn{};
                    fn.worldPos = registry_.get<ecs::Transform>(e).position;
                    fn.amount = -diff;
                    fn.isHeal = false;
                    fn.driftX = static_cast<float>((floatDriftCounter++ % 19) - 9) * 3.5F;
                    floatingNumbers_.push_back(fn);
                } else if (diff >= 0.15F) {
                    FloatingNumber fn{};
                    fn.worldPos = registry_.get<ecs::Transform>(e).position;
                    fn.amount = diff;
                    fn.isHeal = true;
                    fn.driftX = static_cast<float>((floatDriftCounter++ % 19) - 9) * 3.5F;
                    floatingNumbers_.push_back(fn);
                }
            }
            while (floatingNumbers_.size() > 160) {
                floatingNumbers_.erase(floatingNumbers_.begin(),
                                       floatingNumbers_.begin() +
                                           static_cast<std::ptrdiff_t>(floatingNumbers_.size() - 140));
            }
        }
    }

    tickFloatingNumbers(frameDt);

    if (registry_.valid(player_) && registry_.all_of<ecs::Health>(player_)) {
        const float cur = registry_.get<ecs::Health>(player_).current;
        if (cur < prevPlayerHp_ - 1.0e-4F) {
            damageFlashTimer_ = 0.5F;
            const int cix = std::clamp(selectedClass_, 0, std::max(0, characterCount() - 1));
            if (hurtGruntCooldown_ <= 0.001F && characterAt(cix).id == "undead_hunter") {
                Sound grunt = resources.getSound("assets/sounds/undead_hunter_hurt.wav");
                if (IsSoundValid(grunt)) {
                    SetSoundPitch(grunt, 1.0F);
                    PlaySound(grunt);
                }
                hurtGruntCooldown_ = 20.0F;
            }
        }
        prevPlayerHp_ = cur;
    }

    if (!worldBlocked && !paused_) {
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

    if (!worldBlocked) {
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
                    const float spread = 14.0F;

                    int dropIdx = 0;
                    for (const std::string &kind : loadedMap_.casket.itemSlots) {
                        if (kind.empty() || kind == "-") {
                            continue;
                        }
                        ItemData it = makeItemFromMapKind(kind);
                        if (it.name.empty()) {
                            continue;
                        }
                        const int idx = inventory_.addItem(std::move(it));
                        const float ang = static_cast<float>(dropIdx) * 0.85F;
                        const Vector2 drop = {
                            ct.position.x + dir.x * baseDist + perp.x * spread * std::cosf(ang) +
                                static_cast<float>(dropIdx) * 6.0F * dir.x,
                            ct.position.y + dir.y * baseDist + perp.y * spread * std::cosf(ang) +
                                static_cast<float>(dropIdx) * 6.0F * dir.y};
                        spawnItemPickupAtWorld(drop, idx);
                        ++dropIdx;
                    }
                } else if (inter.name == "Anvil") {
                    anvilOpen_ = true;
                    activeAnvil_ = hoveredInteract_;
                    inventoryUi_.setOpen(true);
                    inventoryUi_.setPanelLayoutShift(400.0F);
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
    drawFloatingCombatNumbers(resources);
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
    if (anvilOpen_) {
        drawAnvilUi(resources, buildAnvilUiLayout());
    }

    if (fullMapOpen_) {
        drawMinimapOverlay(true, resources);
    }

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

    buildVisibilityPolygonWorld(playerWorld, registry_, playerVisionRadiusForFog(registry_, player_),
                                fogVisWorld_);

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
        DrawCircleV(centerIso, playerVisionRadiusForFog(registry_, player_), BLACK);
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
    const bool uiBlocksAim =
        paused_ || gameOver_ || inventoryUi_.isOpen() || fullMapOpen_ || !aimScreenInit_;
    const Vector2 m = (!uiBlocksAim) ? aimScreenPos_ : GetMousePosition();
    if (gameOver_ || paused_ || inventoryUi_.isOpen() || fullMapOpen_) {
        drawCustomCursor(resources, CursorKind::Default, GetMousePosition());
        return;
    }
    if (hoveringHudElement_) {
        drawCustomCursor(resources, CursorKind::Default, GetMousePosition());
        return;
    }
    if (registry_.valid(hoveredInteract_) || registry_.valid(hoveredPickup_)) {
        drawCustomCursor(resources, CursorKind::Interact, m);
        return;
    }
    drawCustomCursor(resources, CursorKind::Aim, m);
    if (registry_.valid(player_) && registry_.all_of<ecs::ChamberState>(player_) &&
        resources.settings().showReloadOnCursor) {
        const auto &ch = registry_.get<ecs::ChamberState>(player_);
        if (ch.isReloading && ch.reloadDuration > 1.0e-4F) {
            const float t =
                std::clamp(ch.reloadTimer / ch.reloadDuration, 0.0F, 1.0F);
            const float rOuter = 28.0F;
            const float rInner = rOuter - 5.0F;
            DrawRing(m, rInner, rOuter, -90.0F, -90.0F + 360.0F * t, 40,
                     Color{220, 190, 120, 235});
        }
    }
}

void GameplayScene::drawFogOfWarLegacy() {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Transform>(player_)) {
        return;
    }
    // Fallback when fog RenderTexture/shader init failed (GPU / driver).
    const auto &pt = registry_.get<ecs::Transform>(player_);
    const Vector2 center =
        GetWorldToScreen2D(worldToIso(pt.position), camera_.camera());
    const float inner =
        playerVisionRadiusForFog(registry_, player_) * camera_.camera().zoom;
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

void GameplayScene::tickFloatingNumbers(float frameDt) {
    for (auto &fn : floatingNumbers_) {
        fn.elapsed += frameDt;
    }
    floatingNumbers_.erase(
        std::remove_if(floatingNumbers_.begin(), floatingNumbers_.end(),
                       [](const FloatingNumber &f) { return f.elapsed >= f.lifetime; }),
        floatingNumbers_.end());
}

void GameplayScene::drawFloatingCombatNumbers(ResourceManager &resources) {
    if (floatingNumbers_.empty() || !resources.settings().showDamageNumbers) {
        return;
    }
    const Font &font = resources.uiFont();
    for (const auto &fn : floatingNumbers_) {
        const float t = fn.lifetime > 1.0e-4F ? fn.elapsed / fn.lifetime : 1.0F;
        const float alpha = std::clamp(1.0F - t, 0.0F, 1.0F);
        const Vector2 iso = worldToIso(fn.worldPos);
        Vector2 screen = GetWorldToScreen2D(iso, camera_.camera());
        screen.x += fn.driftX * t;
        screen.y -= 42.0F * t;
        char buf[32];
        if (fn.amount >= 10.0F) {
            std::snprintf(buf, sizeof(buf), "%.0f", static_cast<double>(fn.amount));
        } else {
            std::snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(fn.amount));
        }
        const float fs = 18.0F;
        const Vector2 dim = MeasureTextEx(font, buf, fs, 1.0F);
        const unsigned char a = static_cast<unsigned char>(alpha * 255.0F);
        const Color col =
            fn.isHeal ? Color{110, 255, 150, a} : Color{255, 95, 85, a};
        DrawTextEx(font, buf, {screen.x - dim.x * 0.5F + 1.0F, screen.y - dim.y * 0.5F + 1.0F}, fs,
                   1.0F, Fade(BLACK, alpha * 0.65F));
        DrawTextEx(font, buf, {screen.x - dim.x * 0.5F, screen.y - dim.y * 0.5F}, fs, 1.0F, col);
    }
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
                           case StatusHudKind::ManaRegenOverTime:
                               return !registry_.all_of<ecs::ManaRegenOverTime>(player_);
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

void GameplayScene::tickSlugAim(float fixedDt, ResourceManager &resources) {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::SlugAimState>(player_)) {
        return;
    }
    auto &aim = registry_.get<ecs::SlugAimState>(player_);
    aim.elapsed += fixedDt;
    aim.aimDirection = playerAimDirectionWorld();
    if (aim.elapsed >= aim.aimDuration - 1.0e-4F) {
        const Vector2 d = aim.aimDirection;
        registry_.remove<ecs::SlugAimState>(player_);
        fireCalamitySlug(undeadHunterAbilities().abilities[2], d);
        Sound slugSnd = resources.getSound("assets/sounds/calamity_slug_fire.wav");
        if (IsSoundValid(slugSnd)) {
            SetSoundPitch(slugSnd, 1.0F);
            PlaySound(slugSnd);
        }
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

void GameplayScene::spawnSnareProjectile(const AbilityDef &snareDef, const Vector2 &dirWorld) {
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
    const float throwSpeed = std::max(1.0F, snareDef.projectileSpeed);
    const float throwRange = std::max(1.0F, snareDef.projectileRange);
    const float pullRadius = std::max(1.0F, snareDef.pullRadius);
    const float stunDuration = std::max(0.0F, snareDef.stunDuration);
    const auto proj = registry_.create();
    registry_.emplace<ecs::Transform>(
        proj, ecs::Transform{{pt.position.x + d.x * spawnDist, pt.position.y + d.y * spawnDist},
                             std::atan2f(d.y, d.x)});
    registry_.emplace<ecs::Velocity>(
        proj, ecs::Velocity{{d.x * throwSpeed, d.y * throwSpeed}});
    registry_.emplace<ecs::Sprite>(proj, ecs::Sprite{{90, 85, 140, 255}, 14.0F, 14.0F});
    registry_.emplace<ecs::Projectile>(
        proj,
        ecs::Projectile{0.0F, throwSpeed, throwRange, 0.0F, d, true, entt::null, false, 0.0F});
    registry_.emplace<ecs::SnareProjectile>(
        proj, ecs::SnareProjectile{pullRadius, stunDuration});
}

void GameplayScene::fireCalamitySlug(const AbilityDef &slugDef, const Vector2 &dirWorld) {
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
    const float slugSize = std::max(2.0F, slugDef.projectileSize);
    const float slugSpeed = std::max(1.0F, slugDef.projectileSpeed);
    const float slugRange = std::max(1.0F, slugDef.projectileRange);
    const float slugKnockbackSide = std::max(0.0F, slugDef.knockbackSide);
    const float spawnDist = halfDiag + slugSize * 0.5F + 4.0F;
    const auto proj = registry_.create();
    registry_.emplace<ecs::Transform>(
        proj, ecs::Transform{{pt.position.x + d.x * spawnDist, pt.position.y + d.y * spawnDist},
                             std::atan2f(d.y, d.x)});
    registry_.emplace<ecs::Velocity>(proj, ecs::Velocity{{d.x * slugSpeed, d.y * slugSpeed}});
    registry_.emplace<ecs::Sprite>(proj, ecs::Sprite{{255, 200, 80, 255}, slugSize, slugSize});
    float slugDmg = std::max(0.0F, slugDef.damage);
    if (registry_.valid(player_) && registry_.all_of<ecs::PlayerLevel>(player_)) {
        slugDmg += registry_.get<ecs::PlayerLevel>(player_).rangedDamageBonus;
    }
    registry_.emplace<ecs::Projectile>(
        proj, ecs::Projectile{slugDmg, slugSpeed, slugRange, 0.0F, d, true, entt::null, true, 0.0F});
    registry_.emplace<ecs::SlugProjectile>(
        proj, ecs::SlugProjectile{slugKnockbackSide});
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
    const int ci = std::clamp(selectedClass_, 0, std::max(0, characterCount() - 1));
    if (ci != 0) {
        return;
    }
    const auto &abilities = undeadHunterAbilities().abilities;
    const AbilityDef &def = abilities[static_cast<size_t>(abilityIndex)];
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
        const float leadDuration = std::max(0.1F, def.effectDuration);
        registry_.emplace_or_replace<ecs::LeadFeverEffect>(
            player_, ecs::LeadFeverEffect{leadDuration, 0.0F});
        const std::string icon = def.iconPath;
        pushStatusHud(StatusHudKind::LeadFever, icon, {140, 220, 160, 255});
        return;
    }
    if (abilityIndex == 1) {
        const AbilityDef &snareDef = abilities[1];
        const Vector2 forward = playerAimDirectionWorld();
        spawnSnareProjectile(snareDef, forward);
        const float len = std::sqrt(forward.x * forward.x + forward.y * forward.y);
        const Vector2 f = len > 0.001F ? Vector2{forward.x / len, forward.y / len}
                                       : Vector2{1.0F, 0.0F};
        const float dashSpeed = std::max(1.0F, snareDef.dashSpeed);
        const float dashDistance = std::max(1.0F, snareDef.dashDistance);
        registry_.emplace_or_replace<ecs::SnareDashState>(
            player_, ecs::SnareDashState{dashSpeed, dashDistance, 0.0F, {-f.x, -f.y}});
        return;
    }
    const AbilityDef &slugDef = abilities[2];
    const Vector2 aimDir = playerAimDirectionWorld();
    const float aimDuration = std::max(0.05F, slugDef.aimDuration);
    registry_.emplace_or_replace<ecs::SlugAimState>(
        player_, ecs::SlugAimState{aimDuration, 0.0F, aimDir});
}

void GameplayScene::drawHud(ResourceManager &resources) {
    hoveringHudElement_ = false;
    const Font &font = resources.uiFont();
    const int w = config::WINDOW_WIDTH;
    const int h = config::WINDOW_HEIGHT;

    if (!registry_.valid(player_) || !registry_.all_of<ecs::Health, ecs::Mana>(player_)) {
        return;
    }
    syncStatusHudOrder();
    const auto &hp = registry_.get<ecs::Health>(player_);
    const auto &mp = registry_.get<ecs::Mana>(player_);

    const int ci = std::clamp(selectedClass_, 0, std::max(0, characterCount() - 1));
    const auto &cls = characterAt(ci);

    const float margin = 16.0F;
    const float portraitR = 58.0F;
    const float barW = 380.0F;
    const float barHpH = 34.0F;
    const float barMpH = 22.0F;
    const float barGap = 4.0F;
    const float barX = margin + portraitR * 2.0F + 12.0F;
    const float hudBottomAll = static_cast<float>(h) - margin;
    /// Align equipment row bottom with lower-right consumable row when Undead Hunter HUD is shown.
    const float equipBottomTarget = (ci == 0) ? (hudBottomAll - 8.0F) : (hudBottomAll - 12.0F);
    constexpr float kEquipSlotHeightScale = 0.65F;
    /// HP/mana/equip/portrait Y baseline: chosen so equip bottoms sit on `equipBottomTarget` with
    /// `kEquipSlotHeightScale` and the `max(44, …)` slot-height clamp (see equip row below).
    const float barsStackToEquipTop = barHpH + barGap + barMpH + barGap;
    const float hpBarTop = equipBottomTarget - 44.0F * kEquipSlotHeightScale - barsStackToEquipTop;
    /// Grey backing uses the original layout anchor (do not resize or move the panel).
    const float hpBarTopBacking = hudBottomAll - 138.0F;
    const float icon = 28.0F;
    const float chamberStatusY = hpBarTop - icon - 8.0F;
    drawChamberHudIcon(resources, barX, chamberStatusY, icon);

    const float hudPad = 8.0F;
    const float hudLeft = margin;
    /// Grey backing size does not grow when status icons are shown above the bars.
    const float hudTop = hpBarTopBacking - hudPad;
    const float hudW = barX + barW + hudPad - hudLeft;
    const float hudH = hudBottomAll + hudPad - hudTop;
    DrawRectangle(static_cast<int>(hudLeft - 4.0F), static_cast<int>(hudTop - 4.0F),
                  static_cast<int>(hudW + 8.0F), static_cast<int>(hudH + 8.0F),
                  ui::theme::HUD_GREY_BACKING);

    const float cx = margin + portraitR;
    const float barsTop = hpBarTop;
    const float barsBottom = hpBarTop + barHpH + barGap + barMpH;
    const float cy = barsTop + (barsBottom - barsTop) * 0.5F;
    DrawCircle(static_cast<int>(cx), static_cast<int>(cy), portraitR, ui::theme::PORTRAIT_FILL);
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), portraitR, ui::theme::PORTRAIT_RING);

    const char initial = cls.name.empty() ? '?' : cls.name[0];
    const float portraitFont = 36.0F;
    char oneChar[2] = {initial, '\0'};
    const Vector2 idim = MeasureTextEx(font, oneChar, portraitFont, 1.0F);
    DrawTextEx(font, oneChar, {cx - idim.x * 0.5F, cy - idim.y * 0.5F}, portraitFont, 1.0F,
               RAYWHITE);

    if (registry_.all_of<ecs::PlayerLevel>(player_)) {
        const auto &pl = registry_.get<ecs::PlayerLevel>(player_);
        const float badgeR = 22.0F;
        const float bx = cx + portraitR * 0.58F;
        const float by = cy + portraitR * 0.52F;
        const Vector2 badgeCenter{bx, by};
        DrawCircleV(badgeCenter, badgeR, Color{38, 34, 40, 245});
        DrawCircleLinesV(badgeCenter, badgeR, ui::theme::PORTRAIT_RING);
        const float xpT = pl.xpToNextLevel > 1.0e-4F
                              ? std::clamp(pl.xp / pl.xpToNextLevel, 0.0F, 1.0F)
                              : 0.0F;
        const float startDeg = -90.0F;
        const float progDeg = 360.0F * xpT;
        DrawRing(badgeCenter, badgeR - 3.5F, badgeR - 1.25F, startDeg, startDeg + progDeg, 40,
                 Color{210, 175, 95, 255});
        if (xpT < 0.999F) {
            DrawRing(badgeCenter, badgeR - 3.5F, badgeR - 1.25F, startDeg + progDeg,
                     startDeg + 360.0F, 40, Fade(WHITE, 0.14F));
        }
        char lvlBuf[12];
        std::snprintf(lvlBuf, sizeof(lvlBuf), "%d", pl.level);
        const float lvlFs = 18.0F;
        const Vector2 lvlDim = MeasureTextEx(font, lvlBuf, lvlFs, 1.0F);
        DrawTextEx(font, lvlBuf, {bx - lvlDim.x * 0.5F, by - lvlDim.y * 0.5F}, lvlFs, 1.0F,
                   RAYWHITE);
    }

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

    // Equipped gear row: same vertical gap as between HP and mana (`barGap`). Slot height is a fraction
    // of the space down to the consumable/ability bottom line (`equipBottomTarget`).
    const float equipY = mpBarTop + barMpH + barGap;
    const float equipAvailable = equipBottomTarget - equipY;
    const float equipSpanFull = std::max(44.0F, equipAvailable);
    const float equipSlotH = equipSpanFull * kEquipSlotHeightScale;
    const float equipGap = 6.0F;
    const float equipSlotW = (barW - equipGap * 2.0F) / 3.0F;
    float rscdRatioHud = 0.0F;
    float rscdSecHud = 0.0F;
    if (registry_.valid(player_) && registry_.all_of<ecs::RunicShellCooldown>(player_)) {
        const auto &cd = registry_.get<ecs::RunicShellCooldown>(player_);
        rscdRatioHud = cd.total > 0.001F ? cd.remaining / cd.total : 0.0F;
        rscdSecHud = cd.remaining;
    }
    static const char *kHudEquipSlotIcons[] = {
        "assets/textures/ui/armor_slot.png",
        "assets/textures/ui/amulet_slot.png",
        "assets/textures/ui/ring_slot.png",
    };
    for (int ei = 0; ei < static_cast<int>(EquipSlot::COUNT); ++ei) {
        const float ex = barX + static_cast<float>(ei) * (equipSlotW + equipGap);
        const Rectangle eqR{ex, equipY, equipSlotW, equipSlotH};
        DrawRectangleRec(eqR, ui::theme::SLOT_FILL);
        DrawRectangleLinesEx(eqR, 1.5F, ui::theme::SLOT_BORDER);
        const int eidx = inventory_.equipped[static_cast<size_t>(ei)];
        const ItemData *pit =
            (eidx >= 0 && eidx < static_cast<int>(inventory_.items.size()))
                ? &inventory_.items[static_cast<size_t>(eidx)]
                : nullptr;
        if (pit != nullptr) {
            if (!pit->iconPath.empty()) {
                const Texture2D tex = resources.getTexture(pit->iconPath);
                if (tex.id != 0) {
                    const Rectangle src{0.0F, 0.0F, static_cast<float>(tex.width),
                                        static_cast<float>(tex.height)};
                    const float ip = 2.0F;
                    const float maxW = eqR.width - ip * 2.0F;
                    const float maxH = eqR.height - ip * 2.0F;
                    float dw = maxH * 7.0F / 5.0F;
                    float dh = maxH;
                    if (dw > maxW) {
                        dw = maxW;
                        dh = dw * 5.0F / 7.0F;
                    }
                    const float ix = eqR.x + (eqR.width - dw) * 0.5F;
                    const float iy = eqR.y + (eqR.height - dh) * 0.5F;
                    const Rectangle dst{ix, iy, dw, dh};
                    DrawTexturePro(tex, src, dst, {0.0F, 0.0F}, 0.0F, WHITE);
                }
            }
            if (ei == 0 && rscdRatioHud > 0.001F && pit->name == "Runic Shell") {
                DrawRectangleRec(eqR, Fade(BLACK, 0.45F * rscdRatioHud));
                char cdBuf[16];
                const int sec = static_cast<int>(std::ceil(static_cast<double>(rscdSecHud)));
                std::snprintf(cdBuf, sizeof(cdBuf), "%ds", sec);
                const float cdFs = 16.0F;
                const Vector2 cdDim = MeasureTextEx(font, cdBuf, cdFs, 1.0F);
                DrawTextEx(font, cdBuf,
                           {eqR.x + (eqR.width - cdDim.x) * 0.5F + 1.0F,
                            eqR.y + (eqR.height - cdDim.y) * 0.5F + 1.0F},
                           cdFs, 1.0F, Fade(BLACK, 180));
                DrawTextEx(font, cdBuf,
                           {eqR.x + (eqR.width - cdDim.x) * 0.5F,
                            eqR.y + (eqR.height - cdDim.y) * 0.5F},
                           cdFs, 1.0F, {200, 220, 255, 255});
            }
        } else {
            const Texture2D slotTex = resources.getTexture(kHudEquipSlotIcons[ei]);
            if (slotTex.id != 0 && slotTex.width > 0 && slotTex.height > 0) {
                const float iconFit = std::min(equipSlotW, equipSlotH) * 0.72F;
                const float ix = eqR.x + (eqR.width - iconFit) * 0.5F;
                const float iy = eqR.y + (eqR.height - iconFit) * 0.5F;
                const Rectangle src{0.0F, 0.0F, static_cast<float>(slotTex.width),
                                    static_cast<float>(slotTex.height)};
                const Rectangle dst{ix, iy, iconFit, iconFit};
                DrawTexturePro(slotTex, src, dst, {0.0F, 0.0F}, 0.0F,
                               Fade(ui::theme::MUTED_TEXT, 0.55F));
            }
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
        const float ix = barX + static_cast<float>(j) * (icon + 10.0F);
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
        } else if (hud.kind == StatusHudKind::ManaRegenOverTime &&
                   registry_.all_of<ecs::ManaRegenOverTime>(player_)) {
            const auto &mr = registry_.get<ecs::ManaRegenOverTime>(player_);
            tRem = mr.duration > 0.001F ? std::max(0.0F, 1.0F - mr.elapsed / mr.duration) : 0.0F;
            ringCol = {140, 190, 255, 255};
            fillFallback = {25, 45, 90, 255};
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
        if (hud.kind == StatusHudKind::ManaRegenOverTime && spiritRefreshFlashTimer_ > 0.001F) {
            const float flashA = std::min(1.0F, spiritRefreshFlashTimer_ * 5.0F);
            DrawRectangle(static_cast<int>(ix), static_cast<int>(iy), static_cast<int>(icon),
                          static_cast<int>(icon), Fade(WHITE, 0.4F * flashA));
        }
    }

    // Lower-right: consumable slots (C/V) left of ability bar. Keybind badges drawn last (on top).
    if (ci == 0) {
        constexpr float abSize = 104.0F;
        constexpr float abGap = 16.0F;
        constexpr float consH = 52.0F;
        constexpr float consW = consH * 5.0F / 4.0F;
        constexpr float consGap = 12.0F;
        constexpr float clusterGap = 22.0F;

        const float abRowW = 3.0F * abSize + 2.0F * abGap;
        const float abRight = static_cast<float>(w) - margin;
        const float abX0 = abRight - abRowW;
        const float abY = hudBottomAll - abSize - 8.0F;

        const float consRowW = 2.0F * consW + consGap;
        const float consX0 = abX0 - clusterGap - consRowW;
        // Align bottom edge with ability row (shorter 5:4 slots sit higher).
        const float consY = abY + abSize - consH;

        const float clusterTop = std::min(consY, abY) - 26.0F;
        const float clusterLeft = consX0 - 14.0F;
        const float clusterBottom = std::max(consY + consH, abY + abSize) + 6.0F;
        const float clusterRight = static_cast<float>(w) - margin + 8.0F;
        const Rectangle abilityCluster{clusterLeft, clusterTop,
                                       std::max(8.0F, clusterRight - clusterLeft),
                                       std::max(8.0F, clusterBottom - clusterTop)};
        DrawRectangleRec(abilityCluster, ui::theme::HUD_GREY_BACKING);

        const Vector2 mouse = GetMousePosition();

        bool showHudDescriptionTooltip = false;
        std::string hudDescTitle;
        std::string hudDescBody;

        const char *consKeys[static_cast<int>(dreadcast::CONSUMABLE_SLOT_COUNT)] = {"C", "V"};
        for (int si = 0; si < dreadcast::CONSUMABLE_SLOT_COUNT; ++si) {
            const float sx = consX0 + static_cast<float>(si) * (consW + consGap);
            const Rectangle consR{sx, consY, consW, consH};
            const Rectangle consBadgeR = hudKeyBadgeOuterRect(sx, consY);
            DrawRectangleRec(consR, ui::theme::SLOT_FILL);
            DrawRectangleLinesEx(consR, 1.5F, ui::theme::SLOT_BORDER);

            const int idx = inventory_.consumableSlots[static_cast<size_t>(si)];
            const ItemData *consItem = nullptr;
            if (idx >= 0 && idx < static_cast<int>(inventory_.items.size())) {
                consItem = &inventory_.items[static_cast<size_t>(idx)];
            }
            if (consItem != nullptr) {
                const auto &it = *consItem;
                float ix = sx + 3.0F;
                float iy = consY + 3.0F;
                float dw = consW - 6.0F;
                float dh = consH - 6.0F;
                if (!it.iconPath.empty()) {
                    const Texture2D tex = resources.getTexture(it.iconPath);
                    if (tex.id != 0) {
                        const Rectangle src{0.0F, 0.0F, static_cast<float>(tex.width),
                                            static_cast<float>(tex.height)};
                        const float ip = 3.0F;
                        const float maxW = consW - ip * 2.0F;
                        const float maxH = consH - ip * 2.0F;
                        dw = maxH * 7.0F / 5.0F;
                        dh = maxH;
                        if (dw > maxW) {
                            dw = maxW;
                            dh = dw * 5.0F / 7.0F;
                        }
                        ix = sx + (consW - dw) * 0.5F;
                        iy = consY + (consH - dh) * 0.5F;
                        const Rectangle dst{ix, iy, dw, dh};
                        DrawTexturePro(tex, src, dst, {0.0F, 0.0F}, 0.0F, WHITE);
                        if (it.name == "Vial of Cordial Manic" &&
                            hpRatio < config::MANIC_MIN_HP_FRACTION - 1.0e-4F) {
                            const float m = 4.0F;
                            DrawLineEx({ix + m, iy + m}, {ix + dw - m, iy + dh - m}, 2.5F,
                                       Fade(RED, 210));
                            DrawLineEx({ix + m, iy + dh - m}, {ix + dw - m, iy + m}, 2.5F,
                                       Fade(RED, 210));
                        }
                    }
                }
                if (it.isStackable && it.stackCount >= 1) {
                    char cntBuf[16];
                    std::snprintf(cntBuf, sizeof(cntBuf), "%d", it.stackCount);
                    constexpr float stackFs = 17.0F;
                    const Vector2 cDim = MeasureTextEx(font, cntBuf, stackFs, 1.0F);
                    const float sp = 2.0F;
                    const float tx = ix + dw - cDim.x - sp;
                    const float ty = iy + dh - cDim.y - sp;
                    DrawTextEx(font, cntBuf, {tx + 1.0F, ty + 1.0F}, stackFs, 1.0F,
                               Fade(BLACK, 200));
                    DrawTextEx(font, cntBuf, {tx, ty}, stackFs, 1.0F, RAYWHITE);
                }
            }

            drawHudKeyBadge(font, consKeys[si], sx, consY);
            if (CheckCollisionPointRec(mouse, consR) || CheckCollisionPointRec(mouse, consBadgeR)) {
                hoveringHudElement_ = true;
                if (consItem != nullptr) {
                    showHudDescriptionTooltip = true;
                    hudDescTitle = consItem->name;
                    hudDescBody = consItem->description;
                }
            }
        }

        const char *keyLbl[3] = {"1", "2", "3"};
        for (int ai = 0; ai < 3; ++ai) {
            const float ax = abX0 + static_cast<float>(ai) * (abSize + abGap);
            const Rectangle abR{ax, abY, abSize, abSize};
            const Rectangle abBadgeR = hudKeyBadgeOuterRect(ax, abY);
            const AbilityDef &ad =
                undeadHunterAbilities().abilities[static_cast<size_t>(ai)];
            DrawRectangleRec(abR, ui::theme::SLOT_FILL);
            DrawRectangleLinesEx(abR, 2.0F, ui::theme::SLOT_BORDER);

            if (!ad.iconPath.empty()) {
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

            if (resources.settings().showAbilityManaCost) {
                char mcBuf[16];
                std::snprintf(mcBuf, sizeof(mcBuf), "%.0f",
                              static_cast<double>(ad.manaCost));
                constexpr float mcFs = 24.0F; // MANA COST FONT SIZE
                const Vector2 mcDim = MeasureTextEx(font, mcBuf, mcFs, 1.0F);
                const Color mcCol{80, 140, 255, 255};
                const float mcx = abR.x + abR.width - mcDim.x - 3.0F;
                const float mcy = abR.y + abR.height - mcDim.y - 1.0F;
                DrawTextEx(font, mcBuf, {mcx + 1.0F, mcy + 1.0F}, mcFs, 1.0F, Fade(BLACK, 160));
                DrawTextEx(font, mcBuf, {mcx, mcy}, mcFs, 1.0F, mcCol);
            }

            const float cdTot = ad.cooldown;
            const float cdRem = abilityCdRem_[static_cast<size_t>(ai)];
            if (cdTot > 0.001F && cdRem > 0.001F) {
                const float ratio = std::min(1.0F, cdRem / cdTot);
                DrawRectangleRec(abR, Fade(BLACK, 0.45F * ratio));
                char cdBuf[16];
                std::snprintf(cdBuf, sizeof(cdBuf), "%ds",
                              static_cast<int>(std::ceil(static_cast<double>(cdRem))));
                const float cdFs = 22.0F;
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

            drawHudKeyBadge(font, keyLbl[ai], ax, abY);

            if (CheckCollisionPointRec(mouse, abR) || CheckCollisionPointRec(mouse, abBadgeR)) {
                hoveringHudElement_ = true;
                showHudDescriptionTooltip = true;
                hudDescTitle = ad.name;
                hudDescBody = ad.description;
            }
        }

        if (showHudDescriptionTooltip) {
            const float tipSz = 13.0F;
            const Vector2 titleDim =
                MeasureTextEx(font, hudDescTitle.c_str(), tipSz + 2.0F, 1.0F);
            float descH = 0.0F;
            float descW = 0.0F;
            if (!hudDescBody.empty()) {
                std::istringstream iss(hudDescBody);
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
            Rectangle tip{0.0F, 0.0F, tw, th};
            layoutAbilityDescriptionTooltip(tip, abilityCluster, mouse, w, h);
            DrawRectangleRec(tip, Fade(ui::theme::PANEL_FILL, 235));
            DrawRectangleLinesEx(tip, 1.5F, ui::theme::PANEL_BORDER);
            float yc = tip.y + pad;
            DrawTextEx(font, hudDescTitle.c_str(), {tip.x + pad, yc}, tipSz + 2.0F, 1.0F,
                       RAYWHITE);
            yc += titleDim.y + gap;
            if (!hudDescBody.empty()) {
                std::istringstream iss2(hudDescBody);
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

    drawMinimapOverlay(false, resources);
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
    if (inventoryUi_.isOpen() || paused_ || gameOver_ || fullMapOpen_) {
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
    if (inventoryUi_.isOpen() || paused_ || gameOver_ || fullMapOpen_) {
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
    Rectangle bg{mouse.x, mouse.y - boxH, boxW, boxH};
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

void GameplayScene::tickChamberState(float fixedDt) {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::ChamberState>(player_)) {
        return;
    }
    auto &ch = registry_.get<ecs::ChamberState>(player_);
    if (ch.isReloading) {
        ch.reloadTimer += fixedDt;
        if (ch.reloadTimer >= ch.reloadDuration - 1.0e-4F) {
            ch.shotsRemaining = ch.maxShots;
            ch.isReloading = false;
            ch.reloadTimer = 0.0F;
            ch.idleTimer = 0.0F;
        }
        return;
    }
    if (ch.shotsRemaining < ch.maxShots) {
        ch.idleTimer += fixedDt;
        if (ch.idleTimer >= ch.idleReloadThreshold - 1.0e-4F) {
            ch.shotsRemaining = ch.maxShots;
            ch.idleTimer = 0.0F;
        }
    } else {
        ch.idleTimer = 0.0F;
    }
}

void GameplayScene::drawChamberHudIcon(ResourceManager &resources, float barX, float statusY,
                                        float icon) {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::ChamberState>(player_)) {
        return;
    }
    const auto &ch = registry_.get<ecs::ChamberState>(player_);
    const float ix = barX - icon - 14.0F;
    const float iy = statusY;
    DrawRectangle(static_cast<int>(ix), static_cast<int>(iy), static_cast<int>(icon),
                  static_cast<int>(icon), Color{55, 50, 45, 255});
    DrawRectangleLinesEx({ix, iy, icon, icon}, 3.5F, Color{220, 190, 120, 255});
    const Font &font = resources.uiFont();
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%d", ch.shotsRemaining);
    const float fs = 18.0F;
    const Vector2 d = MeasureTextEx(font, buf, fs, 1.0F);
    DrawTextEx(font, buf, {ix + (icon - d.x) * 0.5F, iy + (icon - d.y) * 0.5F}, fs, 1.0F,
               RAYWHITE);
    const Texture2D ph = resources.getTexture("assets/textures/items/vial_raw_spirit_icon.png");
    if (ph.id != 0) {
        DrawTexturePro(
            ph,
            {0.0F, 0.0F, static_cast<float>(ph.width), static_cast<float>(ph.height)},
            {ix + 2.0F, iy + 2.0F, icon - 4.0F, icon - 4.0F}, {0.0F, 0.0F}, 0.0F,
            Fade(WHITE, 0.22F));
    }
}

void GameplayScene::drawMinimapOverlay(bool fullScreen, ResourceManager &resources) {
    (void)resources;
    float minX = 1.0e9F;
    float maxX = -1.0e9F;
    float minY = 1.0e9F;
    float maxY = -1.0e9F;
    auto expand = [&](float x, float y) {
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    };
    for (const WallData &w : loadedMap_.walls) {
        expand(w.cx - w.halfW, w.cy - w.halfH);
        expand(w.cx + w.halfW, w.cy + w.halfH);
    }
    for (const LavaData &lv : loadedMap_.lavas) {
        expand(lv.cx - lv.halfW, lv.cy - lv.halfH);
        expand(lv.cx + lv.halfW, lv.cy + lv.halfH);
    }
    for (const SolidShapeData &sd : loadedMap_.solidShapes) {
        for (const Vector2 &v : sd.verts) {
            expand(v.x, v.y);
        }
    }
    if (registry_.valid(player_) && registry_.all_of<ecs::Transform>(player_)) {
        const Vector2 p = registry_.get<ecs::Transform>(player_).position;
        expand(p.x, p.y);
    }
    if (minX >= maxX || minY >= maxY) {
        return;
    }
    const float pad = 120.0F;
    minX -= pad;
    maxX += pad;
    minY -= pad;
    maxY += pad;
    const float worldW = maxX - minX;
    const float worldH = maxY - minY;

    const int sw = config::WINDOW_WIDTH;
    const int sh = config::WINDOW_HEIGHT;
    const float mapPx = fullScreen ? static_cast<float>(sw) * 0.72F : 200.0F;
    const float mapPy = fullScreen ? static_cast<float>(sh) * 0.72F : 200.0F;
    const float mapX = fullScreen ? (static_cast<float>(sw) - mapPx) * 0.5F : 16.0F;
    const float mapY = fullScreen ? (static_cast<float>(sh) - mapPy) * 0.5F : 16.0F;

    if (fullScreen) {
        DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.55F));
    }

    DrawRectangle(static_cast<int>(mapX), static_cast<int>(mapY), static_cast<int>(mapPx),
                  static_cast<int>(mapPy), Fade({10, 10, 14, 255}, fullScreen ? 0.92F : 0.88F));
    DrawRectangleLinesEx({mapX, mapY, mapPx, mapPy}, 2.0F, {90, 80, 70, 255});

    auto toMini = [&](Vector2 world) -> Vector2 {
        const float nx = (world.x - minX) / std::max(1.0F, worldW);
        const float ny = (world.y - minY) / std::max(1.0F, worldH);
        return {mapX + nx * mapPx, mapY + ny * mapPy};
    };

    for (const WallData &w : loadedMap_.walls) {
        const Vector2 a = toMini({w.cx - w.halfW, w.cy - w.halfH});
        const Vector2 b = toMini({w.cx + w.halfW, w.cy + w.halfH});
        const float rw = std::max(2.0F, std::fabs(b.x - a.x));
        const float rh = std::max(2.0F, std::fabs(b.y - a.y));
        DrawRectangle(static_cast<int>(std::min(a.x, b.x)), static_cast<int>(std::min(a.y, b.y)),
                      static_cast<int>(rw), static_cast<int>(rh), {70, 62, 55, 220});
    }
    for (const LavaData &lv : loadedMap_.lavas) {
        const Vector2 a = toMini({lv.cx - lv.halfW, lv.cy - lv.halfH});
        const Vector2 b = toMini({lv.cx + lv.halfW, lv.cy + lv.halfH});
        const float rw = std::max(2.0F, std::fabs(b.x - a.x));
        const float rh = std::max(2.0F, std::fabs(b.y - a.y));
        DrawRectangle(static_cast<int>(std::min(a.x, b.x)), static_cast<int>(std::min(a.y, b.y)),
                      static_cast<int>(rw), static_cast<int>(rh), {180, 70, 30, 200});
    }

    Vector2 fogOrigin{0.0F, 0.0F};
    float fogRadius = config::FOG_OF_WAR_RADIUS;
    if (registry_.valid(player_) && registry_.all_of<ecs::Transform>(player_)) {
        fogOrigin = registry_.get<ecs::Transform>(player_).position;
        if (registry_.all_of<ecs::PlayerMoveStats>(player_)) {
            fogRadius = registry_.get<ecs::PlayerMoveStats>(player_).visionRange;
        }
    }

    for (const auto e : registry_.view<ecs::Enemy, ecs::Transform>()) {
        const Vector2 wp = registry_.get<ecs::Transform>(e).position;
        if (!ecs::visible_to_player(registry_, fogOrigin, wp, fogRadius)) {
            continue;
        }
        const Vector2 m = toMini(wp);
        DrawCircleV(m, 4.0F, {220, 70, 70, 255});
    }

    if (loadedMap_.hasCasket) {
        const Vector2 m = toMini({loadedMap_.casket.cx, loadedMap_.casket.cy});
        DrawCircleV(m, 5.0F, {160, 120, 80, 255});
    }
    for (const AnvilData &an : loadedMap_.anvils) {
        const Vector2 m = toMini({an.cx, an.cy});
        DrawRectangle(static_cast<int>(m.x - 4), static_cast<int>(m.y - 4), 8, 8,
                      {200, 180, 120, 255});
    }

    if (registry_.valid(player_) && registry_.all_of<ecs::Transform>(player_)) {
        const Vector2 m = toMini(registry_.get<ecs::Transform>(player_).position);
        DrawCircleV(m, 5.0F, {120, 200, 255, 255});
    }

    if (fullScreen) {
        const Font &font = resources.uiFont();
        DrawTextEx(font, "Map (M / Esc to close)", {mapX + 8.0F, mapY + 8.0F}, 18.0F, 1.0F,
                   Fade(RAYWHITE, 0.85F));
    }
}

void GameplayScene::resetAnvilWorkbench() {
    anvilBenchDragKind_ = AnvilBenchDragKind::None;
    anvilBenchForgeSlot_ = -1;
    anvilBenchDisOutSlot_ = -1;
    if (anvilBenchDragPoolIdx_ >= 0) {
        tryReturnPoolItemToBagOrDrop(anvilBenchDragPoolIdx_);
        anvilBenchDragPoolIdx_ = -1;
    }
    clearDisassembleOutputPool();
    for (int i = 0; i < static_cast<int>(forgeSlots_.size()); ++i) {
        const int idx = forgeSlots_[static_cast<size_t>(i)];
        if (idx < 0) {
            continue;
        }
        const int bag = inventory_.firstEmptyBagSlot();
        if (bag >= 0) {
            inventory_.bagSlots[static_cast<size_t>(bag)] = idx;
        } else {
            spawnItemPickupAtPlayer(idx);
        }
        forgeSlots_[static_cast<size_t>(i)] = -1;
    }
    if (disassembleInputIndex_ >= 0 &&
        disassembleInputIndex_ < static_cast<int>(inventory_.items.size())) {
        const int idx = disassembleInputIndex_;
        const int bag = inventory_.firstEmptyBagSlot();
        if (bag >= 0) {
            inventory_.bagSlots[static_cast<size_t>(bag)] = idx;
        } else {
            spawnItemPickupAtPlayer(idx);
        }
    }
    disassembleInputIndex_ = -1;
}

bool GameplayScene::tryReturnPoolItemToBagOrDrop(int poolIdx) {
    if (poolIdx < 0 || poolIdx >= static_cast<int>(inventory_.items.size())) {
        return false;
    }
    const int bag = inventory_.firstEmptyBagSlot();
    if (bag >= 0) {
        inventory_.bagSlots[static_cast<size_t>(bag)] = poolIdx;
        return true;
    }
    spawnItemPickupAtPlayer(poolIdx);
    return true;
}

void GameplayScene::clearDisassembleOutputPool() {
    for (int i = 0; i < disassembleOutputCount_; ++i) {
        const int idx = disassembleOutputPool_[static_cast<size_t>(i)];
        if (idx >= 0) {
            tryReturnPoolItemToBagOrDrop(idx);
        }
        disassembleOutputPool_[static_cast<size_t>(i)] = -1;
    }
    disassembleOutputCount_ = 0;
}

void GameplayScene::applyAnvilForgeSlotPlace(int slot, int poolIdx) {
    if (slot < 0 || slot >= static_cast<int>(forgeSlots_.size())) {
        return;
    }
    if (poolIdx < 0 || poolIdx >= static_cast<int>(inventory_.items.size())) {
        return;
    }
    const int prev = forgeSlots_[static_cast<size_t>(slot)];
    if (prev >= 0 && prev != poolIdx) {
        tryReturnPoolItemToBagOrDrop(prev);
    }
    forgeSlots_[static_cast<size_t>(slot)] = poolIdx;
}

void GameplayScene::handleInventoryAnvilAction(const ui::InventoryAction &action) {
    if (action.type == ui::InventoryAction::AnvilForgePlace) {
        applyAnvilForgeSlotPlace(action.anvilSlot, action.itemIndex);
    } else if (action.type == ui::InventoryAction::AnvilDisassembleInputPlace) {
        if (disassembleInputIndex_ >= 0 &&
            disassembleInputIndex_ < static_cast<int>(inventory_.items.size())) {
            tryReturnPoolItemToBagOrDrop(disassembleInputIndex_);
        }
        disassembleInputIndex_ = action.itemIndex;
        clearDisassembleOutputPool();
    }
}

void GameplayScene::commitDisassembleRecipe() {
    const auto &drecipes = disassembleRecipes();
    if (drecipes.empty() || disassembleInputIndex_ < 0 ||
        disassembleInputIndex_ >= static_cast<int>(inventory_.items.size())) {
        return;
    }
    if (inventory_.items[static_cast<size_t>(disassembleInputIndex_)].catalogId != drecipes[0].sourceId) {
        return;
    }
    clearDisassembleOutputPool();
    const int stack =
        inventory_.items[static_cast<size_t>(disassembleInputIndex_)].stackCount;
    const int inPool = disassembleInputIndex_;
    inventory_.removeItemAtIndex(inPool);
    disassembleInputIndex_ = -1;
    disassembleOutputCount_ = 0;
    for (const CraftIngredient &outg : drecipes[0].outputs) {
        for (int k = 0; k < outg.count * stack; ++k) {
            ItemData piece = makeItemFromMapKind(outg.itemId);
            if (piece.name.empty()) {
                continue;
            }
            piece.stackCount = 1;
            const int ni = inventory_.addItem(std::move(piece));
            if (disassembleOutputCount_ < static_cast<int>(disassembleOutputPool_.size())) {
                disassembleOutputPool_[static_cast<size_t>(disassembleOutputCount_)] = ni;
                ++disassembleOutputCount_;
            } else {
                spawnItemPickupAtPlayer(ni);
            }
        }
    }
}

ui::AnvilUiLayout GameplayScene::buildAnvilUiLayout() const {
    ui::AnvilUiLayout L{};
    if (!anvilOpen_) {
        return L;
    }
    L.active = true;
    L.forgeTab = anvilTab_ == 0;
    constexpr float panelX = 40.0F;
    constexpr float panelY = 80.0F;
    constexpr float panelW = 300.0F;
    constexpr float panelH = 520.0F;
    L.panelBounds = {panelX, panelY, panelW, panelH};
    const float tabH = 36.0F;
    const float tabY = panelY + 44.0F;
    L.tabForgeRect = {panelX + 8.0F, tabY, panelW * 0.5F - 12.0F, tabH};
    L.tabDisRect = {panelX + panelW * 0.5F + 4.0F, tabY, panelW * 0.5F - 12.0F, tabH};
    const float contentY = tabY + tabH + 16.0F;
    if (L.forgeTab) {
        const auto &recipes = forgeRecipes();
        if (!recipes.empty()) {
            const size_t nIn = recipes[0].inputs.size();
            L.forgeInputCount = static_cast<int>(std::min<size_t>(nIn, L.forgeInputRects.size()));
            const float gap = 10.0F;
            const float innerW = panelW - 32.0F;
            const float slotW =
                L.forgeInputCount > 0 ? (innerW - gap * static_cast<float>(L.forgeInputCount - 1)) /
                                            static_cast<float>(L.forgeInputCount)
                                      : 0.0F;
            float x = panelX + 16.0F;
            for (int i = 0; i < L.forgeInputCount; ++i) {
                L.forgeInputRects[static_cast<size_t>(i)] = {x, contentY, slotW, 78.0F};
                x += slotW + gap;
            }
        }
        L.forgeOutputRect = {panelX + panelW * 0.5F - 50.0F, contentY + 200.0F, 100.0F, 72.0F};
    } else {
        L.disInputRect = {panelX + 40.0F, contentY, panelW - 80.0F, 78.0F};
        L.disBreakRect = {panelX + 40.0F, contentY + 200.0F, panelW - 80.0F, 40.0F};
        const float outY = contentY + 260.0F;
        const float outSlotW = 100.0F;
        const float outGap = 8.0F;
        L.disOutputCount = std::min(disassembleOutputCount_, static_cast<int>(L.disOutputRects.size()));
        float ox = panelX + 20.0F;
        for (int i = 0; i < L.disOutputCount; ++i) {
            L.disOutputRects[static_cast<size_t>(i)] = {ox, outY, outSlotW, 72.0F};
            ox += outSlotW + outGap;
        }
    }
    return L;
}

namespace {

[[nodiscard]] bool forgeSlotsMatchRecipe(const std::array<int, 6> &forgeSlots,
                                         const ForgeRecipe &recipe, const InventoryState &inv) {
    const size_t n = recipe.inputs.size();
    if (n == 0U || n > forgeSlots.size()) {
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        const int idx = forgeSlots[static_cast<size_t>(i)];
        if (idx < 0 || idx >= static_cast<int>(inv.items.size())) {
            return false;
        }
        const auto &it = inv.items[static_cast<size_t>(idx)];
        if (it.catalogId != recipe.inputs[i].itemId) {
            return false;
        }
        if (it.stackCount != recipe.inputs[i].count) {
            return false;
        }
    }
    for (size_t i = n; i < forgeSlots.size(); ++i) {
        if (forgeSlots[static_cast<size_t>(i)] >= 0) {
            return false;
        }
    }
    return true;
}

} // namespace

void GameplayScene::tickAnvilUi(InputManager &input, ResourceManager &resources,
                                const ui::AnvilUiLayout &lay) {
    (void)resources;
    if (!anvilOpen_) {
        return;
    }
    const int sw = config::WINDOW_WIDTH;
    const int sh = config::WINDOW_HEIGHT;
    const Vector2 mouse = input.mousePosition();
    const bool click = input.isMouseButtonPressed(MOUSE_BUTTON_LEFT);
    const bool release = input.isMouseButtonReleased(MOUSE_BUTTON_LEFT);
    const bool rightClick = input.isMouseButtonPressed(MOUSE_BUTTON_RIGHT);

    auto cancelBenchDrag = [&]() {
        if (anvilBenchDragKind_ == AnvilBenchDragKind::ForgeSlot && anvilBenchDragPoolIdx_ >= 0 &&
            anvilBenchForgeSlot_ >= 0 &&
            anvilBenchForgeSlot_ < static_cast<int>(forgeSlots_.size())) {
            forgeSlots_[static_cast<size_t>(anvilBenchForgeSlot_)] = anvilBenchDragPoolIdx_;
        } else if (anvilBenchDragKind_ == AnvilBenchDragKind::DisOut && anvilBenchDragPoolIdx_ >= 0 &&
                   anvilBenchDisOutSlot_ >= 0 &&
                   anvilBenchDisOutSlot_ < static_cast<int>(disassembleOutputPool_.size())) {
            disassembleOutputPool_[static_cast<size_t>(anvilBenchDisOutSlot_)] =
                anvilBenchDragPoolIdx_;
        }
        anvilBenchDragKind_ = AnvilBenchDragKind::None;
        anvilBenchForgeSlot_ = -1;
        anvilBenchDisOutSlot_ = -1;
        anvilBenchDragPoolIdx_ = -1;
    };

    if (click && lay.active) {
        if (CheckCollisionPointRec(mouse, lay.tabForgeRect)) {
            if (anvilTab_ != 0) {
                cancelBenchDrag();
                anvilTab_ = 0;
            }
        } else if (CheckCollisionPointRec(mouse, lay.tabDisRect)) {
            if (anvilTab_ != 1) {
                cancelBenchDrag();
                anvilTab_ = 1;
            }
        }
    }

    if (rightClick && lay.forgeTab && anvilBenchDragKind_ == AnvilBenchDragKind::None &&
        !inventoryUi_.isDragging()) {
        for (int i = 0; i < lay.forgeInputCount; ++i) {
            const Rectangle r = lay.forgeInputRects[static_cast<size_t>(i)];
            if (!CheckCollisionPointRec(mouse, r)) {
                continue;
            }
            const int idx = forgeSlots_[static_cast<size_t>(i)];
            if (idx < 0) {
                continue;
            }
            forgeSlots_[static_cast<size_t>(i)] = -1;
            tryReturnPoolItemToBagOrDrop(idx);
            break;
        }
    }

    if (release && anvilBenchDragKind_ != AnvilBenchDragKind::None) {
        const int poolIdx = anvilBenchDragPoolIdx_;
        if (poolIdx >= 0) {
            bool cleared = false;
            const int bagHit = inventoryUi_.hitTestBagSlot(mouse, sw, sh);
            if (bagHit >= 0 && inventory_.bagSlots[static_cast<size_t>(bagHit)] < 0) {
                inventory_.bagSlots[static_cast<size_t>(bagHit)] = poolIdx;
                cleared = true;
            } else if (anvilBenchDragKind_ == AnvilBenchDragKind::ForgeSlot && lay.forgeTab) {
                for (int i = 0; i < lay.forgeInputCount; ++i) {
                    if (CheckCollisionPointRec(mouse, lay.forgeInputRects[static_cast<size_t>(i)])) {
                        if (forgeSlots_[static_cast<size_t>(i)] < 0) {
                            forgeSlots_[static_cast<size_t>(i)] = poolIdx;
                            cleared = true;
                        }
                        break;
                    }
                }
            } else if (anvilBenchDragKind_ == AnvilBenchDragKind::DisOut) {
                for (int i = 0; i < lay.disOutputCount; ++i) {
                    if (CheckCollisionPointRec(mouse, lay.disOutputRects[static_cast<size_t>(i)])) {
                        if (disassembleOutputPool_[static_cast<size_t>(i)] < 0) {
                            disassembleOutputPool_[static_cast<size_t>(i)] = poolIdx;
                            cleared = true;
                        }
                        break;
                    }
                }
            }
            if (cleared) {
                anvilBenchDragKind_ = AnvilBenchDragKind::None;
                anvilBenchForgeSlot_ = -1;
                anvilBenchDisOutSlot_ = -1;
                anvilBenchDragPoolIdx_ = -1;
            } else {
                cancelBenchDrag();
            }
        } else {
            cancelBenchDrag();
        }
    }

    if (click && !inventoryUi_.isDragging() && anvilBenchDragKind_ == AnvilBenchDragKind::None) {
        if (lay.forgeTab) {
            bool startedBenchDrag = false;
            for (int i = 0; i < lay.forgeInputCount; ++i) {
                const Rectangle r = lay.forgeInputRects[static_cast<size_t>(i)];
                if (!CheckCollisionPointRec(mouse, r)) {
                    continue;
                }
                const int idx = forgeSlots_[static_cast<size_t>(i)];
                if (idx < 0) {
                    continue;
                }
                forgeSlots_[static_cast<size_t>(i)] = -1;
                anvilBenchDragKind_ = AnvilBenchDragKind::ForgeSlot;
                anvilBenchForgeSlot_ = i;
                anvilBenchDragPoolIdx_ = idx;
                startedBenchDrag = true;
                break;
            }
            const auto &recipes = forgeRecipes();
            if (!startedBenchDrag && !recipes.empty() &&
                forgeSlotsMatchRecipe(forgeSlots_, recipes[0], inventory_) &&
                CheckCollisionPointRec(mouse, lay.forgeOutputRect)) {
                ItemData out = makeItemFromMapKind(recipes[0].outputId);
                if (!out.name.empty()) {
                    const size_t nIn = recipes[0].inputs.size();
                    std::vector<int> removeIdx;
                    removeIdx.reserve(nIn);
                    for (size_t k = 0; k < nIn; ++k) {
                        removeIdx.push_back(forgeSlots_[static_cast<size_t>(k)]);
                    }
                    forgeSlots_.fill(-1);
                    std::sort(removeIdx.begin(), removeIdx.end());
                    for (auto it = removeIdx.rbegin(); it != removeIdx.rend(); ++it) {
                        if (*it >= 0 && *it < static_cast<int>(inventory_.items.size())) {
                            inventory_.removeItemAtIndex(*it);
                        }
                    }
                    const int newIdx = inventory_.addItem(std::move(out));
                    const int bag = inventory_.firstEmptyBagSlot();
                    if (bag >= 0) {
                        inventory_.bagSlots[static_cast<size_t>(bag)] = newIdx;
                    } else {
                        spawnItemPickupAtPlayer(newIdx);
                    }
                }
            }
        } else {
            if (disassembleInputIndex_ >= 0 &&
                CheckCollisionPointRec(mouse, lay.disInputRect) &&
                disassembleInputIndex_ < static_cast<int>(inventory_.items.size())) {
                const int idx = disassembleInputIndex_;
                disassembleInputIndex_ = -1;
                tryReturnPoolItemToBagOrDrop(idx);
            }
            if (CheckCollisionPointRec(mouse, lay.disBreakRect)) {
                commitDisassembleRecipe();
            }
            for (int i = 0; i < lay.disOutputCount; ++i) {
                const Rectangle r = lay.disOutputRects[static_cast<size_t>(i)];
                if (!CheckCollisionPointRec(mouse, r)) {
                    continue;
                }
                const int idx = disassembleOutputPool_[static_cast<size_t>(i)];
                if (idx < 0) {
                    continue;
                }
                disassembleOutputPool_[static_cast<size_t>(i)] = -1;
                anvilBenchDragKind_ = AnvilBenchDragKind::DisOut;
                anvilBenchDisOutSlot_ = i;
                anvilBenchDragPoolIdx_ = idx;
                break;
            }
        }
    }
}

void GameplayScene::drawAnvilUi(ResourceManager &resources, const ui::AnvilUiLayout &lay) {
    const Font &font = resources.uiFont();
    constexpr float panelX = 40.0F;
    constexpr float panelY = 80.0F;
    constexpr float panelW = 300.0F;
    constexpr float panelH = 520.0F;
    Color panelFill = ui::theme::PANEL_FILL;
    panelFill.a = 250;
    DrawRectangle(static_cast<int>(panelX), static_cast<int>(panelY), static_cast<int>(panelW),
                  static_cast<int>(panelH), panelFill);
    DrawRectangleLinesEx({panelX, panelY, panelW, panelH}, 2.0F, ui::theme::PANEL_BORDER);
    DrawTextEx(font, "Anvil", {panelX + 12.0F, panelY + 10.0F}, 26.0F, 1.0F, RAYWHITE);

    DrawRectangleRec(lay.tabForgeRect, anvilTab_ == 0 ? ui::theme::BTN_HOVER : ui::theme::SLOT_FILL);
    DrawRectangleRec(lay.tabDisRect, anvilTab_ == 1 ? ui::theme::BTN_HOVER : ui::theme::SLOT_FILL);
    DrawRectangleLinesEx(lay.tabForgeRect, 1.0F, ui::theme::BTN_BORDER);
    DrawRectangleLinesEx(lay.tabDisRect, 1.0F, ui::theme::BTN_BORDER);
    DrawTextEx(font, "Forge", {lay.tabForgeRect.x + 18.0F, lay.tabForgeRect.y + 8.0F}, 18.0F, 1.0F,
               RAYWHITE);
    DrawTextEx(font, "Disassemble", {lay.tabDisRect.x + 8.0F, lay.tabDisRect.y + 8.0F}, 18.0F, 1.0F,
               RAYWHITE);

    if (lay.forgeTab) {
        for (int i = 0; i < lay.forgeInputCount; ++i) {
            const Rectangle r = lay.forgeInputRects[static_cast<size_t>(i)];
            DrawRectangleLinesEx(r, 1.5F, ui::theme::SLOT_BORDER);
            const int idx = forgeSlots_[static_cast<size_t>(i)];
            if (idx >= 0 && idx < static_cast<int>(inventory_.items.size())) {
                const bool ghost =
                    anvilBenchDragKind_ == AnvilBenchDragKind::ForgeSlot && anvilBenchForgeSlot_ == i;
                ui::InventoryUI::drawItemIcon(inventory_.items[static_cast<size_t>(idx)], resources, r,
                                                ghost ? Fade(WHITE, 0.35F) : WHITE);
            }
        }
        DrawTextEx(font, "Drag from inventory into inputs. RMB slot: return to bag.",
                   {panelX + 12.0F, lay.forgeInputRects[0].y + 86.0F}, 13.0F, 1.0F,
                   ui::theme::LABEL_TEXT);

        const float midX = panelX + panelW * 0.5F;
        const float arrowY = lay.forgeInputRects[0].y + 110.0F;
        DrawTriangle({midX, arrowY + 14.0F}, {midX - 10.0F, arrowY}, {midX + 10.0F, arrowY},
                     ui::theme::LABEL_TEXT);

        DrawRectangleLinesEx(lay.forgeOutputRect, 2.0F, ui::theme::BTN_BORDER);
        const auto &recipes = forgeRecipes();
        if (!recipes.empty() && forgeSlotsMatchRecipe(forgeSlots_, recipes[0], inventory_)) {
            const ItemData pv = makeItemFromMapKind(recipes[0].outputId);
            ui::InventoryUI::drawItemIcon(pv, resources, lay.forgeOutputRect, WHITE);
            DrawTextEx(font, "Click output to craft", {panelX + 16.0F, lay.forgeOutputRect.y + 78.0F},
                       13.0F, 1.0F, ui::theme::MUTED_TEXT);
        } else {
            DrawTextEx(font, "Match recipe in inputs", {panelX + 16.0F, lay.forgeOutputRect.y + 28.0F},
                       14.0F, 1.0F, ui::theme::MUTED_TEXT);
        }
    } else {
        DrawRectangleLinesEx(lay.disInputRect, 1.5F, ui::theme::SLOT_BORDER);
        if (disassembleInputIndex_ >= 0 &&
            disassembleInputIndex_ < static_cast<int>(inventory_.items.size())) {
            ui::InventoryUI::drawItemIcon(
                inventory_.items[static_cast<size_t>(disassembleInputIndex_)], resources,
                lay.disInputRect, WHITE);
        }
        DrawTextEx(font, "Drag gear here. Click input to return.", {panelX + 12.0F, lay.disInputRect.y + 86.0F},
                   13.0F, 1.0F, ui::theme::LABEL_TEXT);

        DrawRectangleRec(lay.disBreakRect, ui::theme::BTN_FILL);
        DrawRectangleLinesEx(lay.disBreakRect, 1.0F, ui::theme::BTN_BORDER);
        DrawTextEx(font, "Break down", {lay.disBreakRect.x + lay.disBreakRect.width * 0.5F - 52.0F,
                                        lay.disBreakRect.y + 10.0F},
                   18.0F, 1.0F, RAYWHITE);

        for (int i = 0; i < lay.disOutputCount; ++i) {
            const Rectangle r = lay.disOutputRects[static_cast<size_t>(i)];
            DrawRectangleLinesEx(r, 1.5F, ui::theme::SLOT_BORDER);
            const int pidx = disassembleOutputPool_[static_cast<size_t>(i)];
            if (pidx >= 0 && pidx < static_cast<int>(inventory_.items.size())) {
                const bool ghost = anvilBenchDragKind_ == AnvilBenchDragKind::DisOut &&
                                   anvilBenchDisOutSlot_ == i;
                ui::InventoryUI::drawItemIcon(inventory_.items[static_cast<size_t>(pidx)], resources, r,
                                                ghost ? Fade(WHITE, 0.35F) : WHITE);
            }
        }
        if (disassembleOutputCount_ > 0) {
            DrawTextEx(font, "Drag outputs to inventory", {panelX + 12.0F, lay.disOutputRects[0].y + 78.0F},
                       13.0F, 1.0F, ui::theme::MUTED_TEXT);
        }
    }

    if (anvilBenchDragKind_ != AnvilBenchDragKind::None && anvilBenchDragPoolIdx_ >= 0 &&
        anvilBenchDragPoolIdx_ < static_cast<int>(inventory_.items.size())) {
        const Vector2 m = GetMousePosition();
        const Rectangle ghost{m.x - 36.0F, m.y - 36.0F, 72.0F, 72.0F};
        ui::InventoryUI::drawItemIcon(inventory_.items[static_cast<size_t>(anvilBenchDragPoolIdx_)],
                                      resources, ghost, Fade(WHITE, 0.9F));
    }
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
