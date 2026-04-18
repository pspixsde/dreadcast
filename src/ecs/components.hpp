#pragma once

#include <string>
#include <vector>

#include <entt/entt.hpp>
#include <raylib.h>

#include "config.hpp"

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
    /// Enemy shots: entity that fired (for damage reflect). `entt::null` if unknown.
    entt::entity source{entt::null};
    /// If true, projectile is not destroyed when it hits an enemy (Calamity Slug).
    bool pierce{false};
    /// If > 0, player shots apply knockback along `direction` (Lead Fever pellets).
    float knockbackOnHit{0.0F};
};

struct MeleeAttacker {
    float damage{dreadcast::config::MELEE_DAMAGE};
    float knockback{dreadcast::config::MELEE_KNOCKBACK};
    float range{dreadcast::config::MELEE_RANGE};
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

    static constexpr float kKnockbackScale[3] = {0.9F, 1.1F, 1.6F};
    static constexpr float kDamageScale[3] = {1.0F, 1.0F, 1.0F};
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
    Vector2 prevPosition{0.0F, 0.0F};
    float stuckTimer{0.0F};
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

/// Walk-through lava hazard (`Transform::position` is center).
struct Lava {
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

/// Vial of Raw Spirit: mana restored over time (one active instance; re-applying replaces).
struct ManaRegenOverTime {
    float totalMana{50.0F};
    float duration{6.0F};
    float elapsed{0.0F};
    float regenedSoFar{0.0F};
};

/// Vial of Cordial Manic: speed + invuln + HP drain, blocks external regen / HOT.
struct ManicEffect {
    float duration{7.0F};
    float elapsed{0.0F};
    float hpDrainTotal{0.0F};
    float hpDrained{0.0F};
};

/// Active knockback: enemy skips AI and decelerates until elapsed >= duration.
struct KnockbackState {
    float duration{0.25F};
    float elapsed{0.0F};
};

/// Runic Shell triggered-ability cooldown on the player.
struct RunicShellCooldown {
    float remaining{0.0F};
    float total{30.0F};
};

/// Enemy cannot act until elapsed &gt;= duration (AI skipped). Knockback still applies.
struct StunnedState {
    float duration{2.0F};
    float elapsed{0.0F};
};

/// Undead Hunter ability 1: multi-pellet ranged with knockback.
struct LeadFeverEffect {
    float duration{6.0F};
    float elapsed{0.0F};
};

/// Deadlight Snare net projectile: on first enemy hit, pull and stun in radius.
struct SnareProjectile {
    float pullRadius{100.0F};
    float stunDuration{2.0F};
};

/// Calamity Slug: piercing shot with sideways knockback.
struct SlugProjectile {
    float sideKnockback{400.0F};
};

/// Tracks enemies already damaged by one piercing slug (one hit each).
struct PierceHitRecord {
    std::vector<entt::entity> hit{};
};

/// Player channeling Calamity Slug before firing.
struct SlugAimState {
    float aimDuration{1.0F};
    float elapsed{0.0F};
    Vector2 aimDirection{0.0F, 0.0F};
};

/// Player dashing backward after throwing snare.
struct SnareDashState {
    float speed{800.0F};
    float distance{150.0F};
    float traveled{0.0F};
    Vector2 direction{0.0F, 0.0F};
};

/// Base ranged damage from character data (curse bolt); level bonuses add on top.
struct PlayerCombatBase {
    float rangedDamage{10.0F};
};

/// Movement + fog vision from character data (JSON).
struct PlayerMoveStats {
    float moveSpeed{dreadcast::config::PLAYER_MOVE_SPEED};
    float visionRange{dreadcast::config::FOG_OF_WAR_RADIUS};
};

/// Max HP/Mana before equipment (from character JSON); equipment adds on top.
struct PlayerClassStats {
    float baseMaxHp{100.0F};
    float baseMaxMana{100.0F};
};

/// XP and per-level tuning (gains copied from character at spawn).
struct PlayerLevel {
    int level{1};
    /// Spent via skill tree; +1 on each level-up.
    int skillPoints{0};
    float xp{0.0F};
    float xpToNextLevel{100.0F};
    /// Extra ranged damage from level-ups (added to `PlayerCombatBase::rangedDamage`).
    float rangedDamageBonus{0.0F};
    float perLevelMaxHp{10.0F};
    float perLevelMaxMana{10.0F};
    float perLevelRangedDamage{5.0F};
    float perLevelMeleeDamage{5.0F};
};

/// XP granted when this enemy dies.
struct EnemyXpReward {
    float xp{25.0F};
};

/// Display level on enemy HUD (current content: all level 1).
struct EnemyDisplayLevel {
    int level{1};
};

/// Ranged chamber / reload (Undead Hunter basic shots).
struct ChamberState {
    int shotsRemaining{config::CHAMBER_MAX_SHOTS};
    int maxShots{config::CHAMBER_MAX_SHOTS};
    float reloadTimer{0.0F};
    float reloadDuration{config::CHAMBER_RELOAD_TIME};
    bool isReloading{false};
    float idleTimer{0.0F};
    float idleReloadThreshold{config::CHAMBER_IDLE_RELOAD_TIME};
};

/// Solid obstacle from map editor (closed polygon in world space).
struct SolidPolygon {
    std::vector<Vector2> vertsWorld{};
};

/// Tag for the player-controlled entity.
struct Player {};

/// Tag for hostile entities (Imps).
struct Enemy {};

} // namespace dreadcast::ecs
