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
    bool rmbHeldPrev{false};

    enum class Phase { Idle, Swing, BetweenSwings, Recovery } phase{Phase::Idle};
    /// Active swing in the 3-hit string: 0, 1, or 2.
    int swingIndex{0};
    float phaseTimer{0.0F};
    bool hitAppliedThisSwing{false};
    /// Blocks starting a new swing immediately after a single-strike from a tap.
    float singleSwingCooldownTimer{0.0F};

    static constexpr float kSwingDuration[3] = {0.20F, 0.20F, 0.28F};
    static constexpr float kHitWindowStart = 0.32F;
    static constexpr float kHitWindowEnd = 0.58F;
    static constexpr float kBetweenSwingsPause = 0.07F;
    static constexpr float kRecoveryDuration = 0.38F;
    static constexpr float kSingleSwingCooldown = 0.22F;

    static constexpr float kKnockbackScale[3] = {0.6F, 0.8F, 1.5F};
    static constexpr float kDamageScale[3] = {1.0F, 1.0F, 1.35F};
    static constexpr float kArcHalfDeg[3] = {35.0F, 45.0F, 60.0F};

    [[nodiscard]] bool isMeleeArcActive() const { return phase == Phase::Swing; }
};

struct NameTag {
    const char *name{"Unknown"};
};

enum class EnemyType : int { Imp = 0, Hellhound = 1 };

struct EnemyAI {
    EnemyType type{EnemyType::Imp};
    float shootCooldown{2.0F};
    float shootTimer{0.0F};
    float minShootRange{60.0F};
    float meleeDamage{0.0F};
    float meleeRange{0.0F};
    float meleeCooldown{1.0F};
    float meleeTimer{0.0F};
    float chaseSpeed{0.0F};
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

/// Enemy becomes active when player enters `agitationRange`; calms after `calmDownDelay` without
/// a fresh sighting, or immediately when giving up at last known position behind cover.
struct Agitation {
    float agitationRange{350.0F};
    float calmDownDelay{5.0F};
    float calmDownTimer{0.0F};
    bool agitated{false};
    Vector2 lastKnownPlayerPos{0.0F, 0.0F};
    bool hasLastKnownPos{false};
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
