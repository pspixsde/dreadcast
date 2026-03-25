#include "scenes/gameplay_scene.hpp"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <memory>

#include <raylib.h>

#include "ui/theme.hpp"

#include "config.hpp"
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
#include "scenes/menu_scene.hpp"
#include "scenes/scene_manager.hpp"
#include "scenes/settings_scene.hpp"

namespace dreadcast {

namespace {

void spawnWallBlock(entt::registry &reg, float cx, float cy, float halfW, float halfH) {
    const auto e = reg.create();
    reg.emplace<ecs::Transform>(e, ecs::Transform{{cx, cy}, 0.0F});
    reg.emplace<ecs::Wall>(e, ecs::Wall{halfW, halfH});
}

} // namespace

GameplayScene::GameplayScene(int selectedClassIndex) : selectedClass_(selectedClassIndex) {}

void GameplayScene::onEnter() {
    camera_.init(config::WINDOW_WIDTH, config::WINDOW_HEIGHT);
    if (!spawned_) {
        spawnWorld();
        spawned_ = true;
    }
}

void GameplayScene::onExit() {}

void GameplayScene::spawnWalls() {
    // Safe area (player ~(-100,0)): west, north, south, east split (gap at y≈0)
    spawnWallBlock(registry_, -220.0F, 0.0F, 22.0F, 190.0F);
    spawnWallBlock(registry_, -40.0F, -210.0F, 220.0F, 22.0F);
    spawnWallBlock(registry_, -40.0F, 210.0F, 220.0F, 22.0F);
    spawnWallBlock(registry_, 180.0F, 95.0F, 22.0F, 75.0F);
    spawnWallBlock(registry_, 180.0F, -95.0F, 22.0F, 75.0F);

    // Imp arena bounds (north/south/east; west opens toward safe zone).
    // East wall is split with a real gap at y≈0 (two halfH=120 blocks meet flush and seal the door).
    spawnWallBlock(registry_, 520.0F, -320.0F, 420.0F, 22.0F);
    spawnWallBlock(registry_, 520.0F, 320.0F, 420.0F, 22.0F);
    spawnWallBlock(registry_, 930.0F, 135.0F, 22.0F, 100.0F);
    spawnWallBlock(registry_, 930.0F, -135.0F, 22.0F, 100.0F);

    // Casket alcove (east)
    spawnWallBlock(registry_, 1040.0F, -210.0F, 220.0F, 22.0F);
    spawnWallBlock(registry_, 1040.0F, 210.0F, 220.0F, 22.0F);
    spawnWallBlock(registry_, 1200.0F, 0.0F, 22.0F, 180.0F);
    spawnWallBlock(registry_, 880.0F, 120.0F, 22.0F, 70.0F);
    spawnWallBlock(registry_, 880.0F, -120.0F, 22.0F, 70.0F);
}

void GameplayScene::spawnWorld() {
    registry_.clear();

    player_ = registry_.create();
    registry_.emplace<ecs::Transform>(player_, ecs::Transform{{-100.0F, 0.0F}, 0.0F});
    registry_.emplace<ecs::Velocity>(player_, ecs::Velocity{});
    registry_.emplace<ecs::Sprite>(player_,
                                   ecs::Sprite{{0, 220, 255, 255}, 36.0F, 36.0F});
    registry_.emplace<ecs::Health>(player_, ecs::Health{config::PLAYER_BASE_MAX_HP,
                                                        config::PLAYER_BASE_MAX_HP});
    registry_.emplace<ecs::Mana>(player_, ecs::Mana{100.0F, 100.0F});
    registry_.emplace<ecs::MeleeAttacker>(player_, ecs::MeleeAttacker{});
    registry_.emplace<ecs::Facing>(player_, ecs::Facing{});
    registry_.emplace<ecs::Player>(player_);

    spawnWalls();

    const Vector2 enemyPositions[] = {{450.0F, -200.0F}, {400.0F, 150.0F}, {650.0F, 100.0F}};
    for (const Vector2 &p : enemyPositions) {
        const auto e = registry_.create();
        registry_.emplace<ecs::Transform>(e, ecs::Transform{p, 0.0F});
        registry_.emplace<ecs::Velocity>(e, ecs::Velocity{});
        registry_.emplace<ecs::Sprite>(
            e, ecs::Sprite{{220, 90, 60, 255}, 72.0F, 72.0F});
        registry_.emplace<ecs::Facing>(e, ecs::Facing{});
        registry_.emplace<ecs::Health>(e, ecs::Health{50.0F, 50.0F});
        registry_.emplace<ecs::Enemy>(e);
        registry_.emplace<ecs::NameTag>(e, ecs::NameTag{"Imp"});
        registry_.emplace<ecs::EnemyAI>(
            e, ecs::EnemyAI{config::IMP_SHOOT_COOLDOWN, config::IMP_SHOOT_COOLDOWN,
                            config::IMP_MIN_SHOOT_RANGE});
        registry_.emplace<ecs::Agitation>(
            e, ecs::Agitation{config::IMP_AGITATION_RANGE, config::ENEMY_CALM_DOWN_DELAY, 0.0F,
                              false});
    }

    const auto casket = registry_.create();
    registry_.emplace<ecs::Transform>(casket, ecs::Transform{{1050.0F, 0.0F}, 0.0F});
    registry_.emplace<ecs::Velocity>(casket, ecs::Velocity{});
    registry_.emplace<ecs::Sprite>(casket, ecs::Sprite{{90, 70, 55, 255}, 56.0F, 40.0F});
    registry_.emplace<ecs::Interactable>(casket, ecs::Interactable{"Old Casket", false});

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
    hp.max = newMax;
    if (hp.current > hp.max) {
        hp.current = hp.max;
    }
}

void GameplayScene::tickHealOverTime(float fixedDt) {
    if (!registry_.valid(player_) || !registry_.all_of<ecs::Health>(player_)) {
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
    if (it.name != "Vial of Pure Blood") {
        return;
    }
    if (registry_.valid(player_) && registry_.all_of<ecs::HealOverTime>(player_)) {
        return;
    }
    it.stackCount -= 1;
    if (it.stackCount <= 0) {
        inventory_.consumableSlots[static_cast<size_t>(slotIndex)] = -1;
        inventory_.removeItemAtIndex(idx);
    }
    applyVialHealOverTime();
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
    if (it.name != "Vial of Pure Blood") {
        return;
    }
    if (registry_.valid(player_) && registry_.all_of<ecs::HealOverTime>(player_)) {
        return;
    }
    it.stackCount -= 1;
    if (it.stackCount <= 0) {
        inventory_.bagSlots[static_cast<size_t>(bagSlot)] = -1;
        inventory_.removeItemAtIndex(idx);
    }
    applyVialHealOverTime();
}

void GameplayScene::applyVialHealOverTime() {
    if (!registry_.valid(player_)) {
        return;
    }
    registry_.emplace_or_replace<ecs::HealOverTime>(
        player_, ecs::HealOverTime{config::HOT_TOTAL_HEAL, config::HOT_DURATION, 0.0F, 0.0F});
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

void GameplayScene::update(SceneManager &scenes, InputManager &input,
                           [[maybe_unused]] ResourceManager &resources, float frameDt) {
    noManaFlashTimer_ = std::max(0.0F, noManaFlashTimer_ - frameDt);
    inventoryFullFlashTimer_ = std::max(0.0F, inventoryFullFlashTimer_ - frameDt);
    damageFlashTimer_ = std::max(0.0F, damageFlashTimer_ - frameDt);

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
            inventoryUi_.toggle();
        }
    }

    if (inventoryUi_.isOpen()) {
        if (input.isKeyPressed(KEY_ESCAPE)) {
            inventoryUi_.setOpen(false);
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

    const bool invOpen = inventoryUi_.isOpen();
    if (!invOpen) {
        ecs::input_system(registry_, input, camera_.camera());
        ecs::combat_player_ranged(registry_, input, camera_.camera(), player_, noManaFlashTimer_);
    } else if (registry_.valid(player_) && registry_.all_of<ecs::Velocity>(player_)) {
        auto &vel = registry_.get<ecs::Velocity>(player_);
        vel.value.x = 0.0F;
        vel.value.y = 0.0F;
    }

    const int steps = fixedTimer_.consumeSteps(frameDt);
    for (int i = 0; i < steps; ++i) {
        if (!invOpen) {
            ecs::combat_player_melee(registry_, input, player_, config::FIXED_DT);
        } else if (registry_.valid(player_) && registry_.all_of<ecs::MeleeAttacker>(player_)) {
            auto &melee = registry_.get<ecs::MeleeAttacker>(player_);
            melee.isAttacking = false;
            melee.swingPhase = 0.0F;
        }
        ecs::enemy_ai_system(registry_, config::FIXED_DT);
        ecs::movement_system(registry_, config::FIXED_DT);
        ecs::wall_resolve_collisions(registry_);
        ecs::wall_destroy_projectiles(registry_);
        ecs::projectile_system(registry_, config::FIXED_DT);
        ecs::collision::projectile_hits(registry_);
        ecs::collision::player_pickup_mana_shards(registry_, player_);
        ecs::death_system(registry_, player_, &enemiesSlain_);
        tickHealOverTime(config::FIXED_DT);

        // Passive regen based on the selected class.
        if (registry_.valid(player_) && registry_.all_of<ecs::Health, ecs::Mana>(player_)) {
            const int ci = std::clamp(selectedClass_, 0, CLASS_COUNT - 1);
            const auto &cls = AVAILABLE_CLASSES[static_cast<size_t>(ci)];
            auto &hp2 = registry_.get<ecs::Health>(player_);
            auto &mp = registry_.get<ecs::Mana>(player_);
            hp2.current = std::min(hp2.max, hp2.current + cls.hpRegen * config::FIXED_DT);
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
    }

    if (!invOpen) {
        if (registry_.valid(player_) && registry_.all_of<ecs::Transform>(player_)) {
            const Vector2 ppos = registry_.get<ecs::Transform>(player_).position;
            const Vector2 wm = worldMouseFromScreen(input.mousePosition());
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
                    ItemData armor{};
                    armor.name = "Iron Armor";
                    armor.slot = EquipSlot::Armor;
                    armor.maxHpBonus = 10.0F;
                    armor.description = "+10 Max HP";
                    const int armorIdx = inventory_.addItem(std::move(armor));

                    ItemData vile{};
                    vile.name = "Vial of Pure Blood";
                    vile.isConsumable = true;
                    vile.isStackable = true;
                    vile.maxStack = 5;
                    vile.stackCount = 1;
                    vile.description = "Regenerates 40 HP over 8 seconds";
                    const int vileIdx = inventory_.addItem(std::move(vile));

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

    DrawRectangle(0, 0, w, h, ui::theme::CLEAR_BG);

    BeginMode2D(camera_.camera());

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
    drawLootPickupHighlight(resources);

    EndMode2D();

    drawDamageVignette();
    drawHud(resources);
    drawFlashMessages(resources);

    inventoryUi_.draw(resources.uiFont(), w, h, inventory_);

    drawLootProximityPrompt(resources);

    if (paused_) {
        drawPauseOverlay(resources);
    }
    if (gameOver_) {
        drawGameOverScreen(resources);
    }
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

void GameplayScene::drawHud(ResourceManager &resources) {
    const Font &font = resources.uiFont();
    const int w = config::WINDOW_WIDTH;
    const int h = config::WINDOW_HEIGHT;

    if (!registry_.valid(player_) || !registry_.all_of<ecs::Health, ecs::Mana>(player_)) {
        return;
    }
    const auto &hp = registry_.get<ecs::Health>(player_);
    const auto &mp = registry_.get<ecs::Mana>(player_);

    const float margin = 16.0F;
    const float portraitR = 42.0F;
    const float barW = 280.0F;
    const float barHpH = 28.0F;
    const float barMpH = 18.0F;
    const float barGap = 4.0F;
    const float barX = margin + portraitR * 2.0F + 8.0F;
    const float hudBottomAll = static_cast<float>(h) - margin;
    const float consumableRowH = 44.0F;
    const float hudBottom = hudBottomAll - consumableRowH;
    const float hpBarTop = hudBottom - barMpH - barGap - barHpH;
    const float icon = 28.0F;
    const bool hotActive = registry_.all_of<ecs::HealOverTime>(player_);
    const float iconLift = hotActive ? icon + 10.0F : 0.0F;

    const float hudPad = 8.0F;
    const float hudLeft = margin;
    const float hudTop = hpBarTop - hudPad - iconLift;
    const float hudW = barX + barW + hudPad - hudLeft;
    const float hudH = hudBottomAll + hudPad - hudTop;
    DrawRectangle(static_cast<int>(hudLeft - 4.0F), static_cast<int>(hudTop - 4.0F),
                  static_cast<int>(hudW + 8.0F), static_cast<int>(hudH + 8.0F), ui::theme::HUD_BACKING);

    const float cx = margin + portraitR;
    const float cy = hudBottom - portraitR;
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

    // Passive regen indicator (e.g., "+1.0").
    if (cls.hpRegen > 0.001F) {
        char regenBuf[32];
        std::snprintf(regenBuf, sizeof(regenBuf), "+%.1f", static_cast<double>(cls.hpRegen));
        const float regenSz = 13.0F;
        const Vector2 regenDim = MeasureTextEx(font, regenBuf, regenSz, 1.0F);
        DrawTextEx(font, regenBuf,
                   {barX + barW - regenDim.x - 4.0F,
                    hpBarTop + (barHpH - regenDim.y) * 0.5F},
                   regenSz, 1.0F, ui::theme::LABEL_TEXT);
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
            std::string shortName = it.name;
            const size_t sp = shortName.find(' ');
            if (sp != std::string::npos) {
                shortName.resize(sp);
            }
            DrawTextEx(font, shortName.c_str(), {sx + 34.0F, slotY + 5.0F}, 14.0F, 1.0F,
                       ui::theme::LABEL_TEXT);

            char cntBuf[16];
            std::snprintf(cntBuf, sizeof(cntBuf), "%d", count);
            const Vector2 cDim = MeasureTextEx(font, cntBuf, 14.0F, 1.0F);
            DrawTextEx(font, cntBuf, {sx + slotW - cDim.x - 6.0F, slotY + 6.0F}, 14.0F, 1.0F,
                       RAYWHITE);
        }
    }

    if (hotActive) {
        const auto &hot = registry_.get<ecs::HealOverTime>(player_);
        const float tRem =
            hot.duration > 0.001F ? std::max(0.0F, 1.0F - hot.elapsed / hot.duration) : 0.0F;
        const float ix = barX;
        const float iy = hpBarTop - icon - 8.0F;
        DrawRectangle(static_cast<int>(ix), static_cast<int>(iy), static_cast<int>(icon),
                      static_cast<int>(icon), {120, 20, 30, 255});
        DrawRectangleLines(static_cast<int>(ix), static_cast<int>(iy), static_cast<int>(icon),
                           static_cast<int>(icon), {220, 80, 80, 255});
        const float thick = 3.0F * tRem;
        if (thick > 0.05F) {
            DrawRectangleLinesEx(
                Rectangle{ix - thick, iy - thick, icon + thick * 2.0F, icon + thick * 2.0F}, thick,
                Fade({255, 200, 120, 255}, 0.85F));
        }
    }

    (void)w;
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
    const auto &pickupT = registry_.get<ecs::Transform>(hoveredPickup_);
    const char *name = inventory_.items[static_cast<size_t>(idx)].name.c_str();
    const float nameSz = 16.0F;
    const char *prompt = "Press [E] to pick up";
    const float promptSz = 18.0F;
    const Vector2 iso = worldToIso(pickupT.position);
    const Vector2 screen = GetWorldToScreen2D(iso, camera_.camera());
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
    const float bx = screen.x - boxW * 0.5F;
    const float by = screen.y - 52.0F;
    const Rectangle bg{bx, by, boxW, boxH};
    DrawRectangleRec(bg, {0, 0, 0, 210});
    DrawRectangleLinesEx(bg, 2.0F, ui::theme::BTN_BORDER);
    DrawTextEx(font, name, {bx + 12.0F, by + 8.0F}, nameSz, 1.0F, RAYWHITE);
    float textY = by + 16.0F + nameDim.y;
    if (altHeld && !desc.empty()) {
        const float descSz = 14.0F;
        DrawTextEx(font, desc.c_str(), {bx + 12.0F, textY}, descSz, 1.0F, ui::theme::LABEL_TEXT);
        textY += descDim.y + 6.0F;
    }
    DrawTextEx(font, prompt, {bx + (boxW - prDim.x) * 0.5F, textY}, promptSz, 1.0F,
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
