#pragma once

#include <string>

#include <raylib.h>

namespace dreadcast::ecs {

struct Transform {
    Vector2 position{0.0F, 0.0F};
    float rotation{0.0F};
};

struct Velocity {
    Vector2 value{0.0F, 0.0F};
};

struct Sprite {
    Color tint{WHITE};
    float width{32.0F};
    float height{32.0F};
};

/// 8-directional facing (E, NE, N, NW, W, SW, S, SE) for isometric placeholders.
enum class FacingDir : int { E = 0, NE, N, NW, W, SW, S, SE };

struct Facing {
    FacingDir dir{FacingDir::S};
};

/// Static texture path for sprite drawing; falls back to `Sprite::tint` rect if loading fails.
struct TextureSprite {
    std::string path{};
};

/// Simple looping spritesheet animation (horizontal frames on one row).
struct SpriteAnimation {
    std::string spritesheetPath{};
    int frameWidth{0};
    int frameHeight{0};
    int frameCount{1};
    float fps{8.0F};
};

struct Health {
    float current{100.0F};
    float max{100.0F};
};

struct Mana {
    float current{100.0F};
    float max{100.0F};
};

struct Projectile {
    float damage{10.0F};
    float speed{600.0F};
    float maxRange{800.0F};
    float distanceTraveled{0.0F};
    Vector2 direction{};
    bool fromPlayer{true};
};

struct MeleeAttacker {
    float damage{20.0F};
    float knockback{200.0F};
    float range{60.0F};
    float cooldown{0.4F};
    float cooldownTimer{0.0F};
    bool isAttacking{false};
    // Prevent right-click spamming: release starts a short cooldown,
    // but keeping RMB held keeps the attack going.
    float reEngageCooldown{0.0F};
    bool rmbHeldPrev{false};
    float swingPhase{0.0F};
};

struct NameTag {
    const char *name{"Unknown"};
};

struct EnemyAI {
    float shootCooldown{2.0F};
    float shootTimer{0.0F};
    float minShootRange{60.0F};
};

struct ManaShard {
    float manaAmount{20.0F};
};

/// Ground loot; `itemIndex` indexes into `InventoryState::items`.
struct ItemPickup {
    int itemIndex{-1};
};

/// Axis-aligned wall block in world space (`Transform::position` is center).
struct Wall {
    float halfW{32.0F};
    float halfH{32.0F};
};

/// Enemy becomes active when player enters `agitationRange`; calms after `calmDownDelay` outside.
struct Agitation {
    float agitationRange{350.0F};
    float calmDownDelay{5.0F};
    float calmDownTimer{0.0F};
    bool agitated{false};
};

/// World interactable (casket, etc.); `name` shown in prompt.
struct Interactable {
    std::string name{"Unknown"};
    bool opened{false};
};

/// Smooth heal-over-time on the player (one active instance; re-applying replaces).
struct HealOverTime {
    float totalHeal{40.0F};
    float duration{8.0F};
    float elapsed{0.0F};
    float healedSoFar{0.0F};
};

/// Tag for the player-controlled entity.
struct Player {};

/// Tag for hostile entities (Imps).
struct Enemy {};

} // namespace dreadcast::ecs
