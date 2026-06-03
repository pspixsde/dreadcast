#pragma once

#include <array>
#include <string>
#include <variant>
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

enum class EnemyType : int { Imp = 0, Hellhound = 1, Warden = 2, Dreg = 3 };

/// Maps `assets/data/enemies.json` `behavior` string to runtime AI branch.
inline EnemyType enemyTypeFromBehavior(const std::string &behavior) {
    if (behavior == "melee_chaser") {
        return EnemyType::Hellhound;
    }
    if (behavior == "mid_bruiser") {
        return EnemyType::Warden;
    }
    if (behavior == "dreg_swarmer") {
        return EnemyType::Dreg;
    }
    return EnemyType::Imp;
}

struct EnemyAI {
    EnemyType type{EnemyType::Imp};
    float shootCooldown{2.0F};
    float shootTimer{0.0F};
    float minShootRange{60.0F};
    float preferredRange{220.0F};
    float kiteSpeed{145.0F};
    float advanceSpeed{100.0F};
    float panicRange{80.0F};
    float strafeBias{0.4F};
    float projectileDamage{15.0F};
    float projectileSpeed{400.0F};
    float projectileRange{400.0F};
    float meleeDamage{0.0F};
    float meleeRange{0.0F};
    float meleeCooldown{1.0F};
    float meleeTimer{0.0F};
    float chaseSpeed{0.0F};
    Vector2 prevPosition{0.0F, 0.0F};
    float stuckTimer{0.0F};
    /// Relentless aggro that never calms and always tracks the live player position (Node-spawned
    /// Dregs). Bypasses agitation range / line-of-sight calm-down.
    bool permanentAggro{false};
};

/// Temporary movement-speed multiplier on an enemy (e.g. Dregs bursting out of a Node). Removed
/// when `elapsed >= duration`.
struct EnemySpeedBuff {
    float multiplier{1.5F};
    float duration{2.0F};
    float elapsed{0.0F};
};

/// Tag for entities that should not be displaced by knockback or unit separation (e.g. Nodes).
struct Immovable {};

/// Stationary Dreg spawner. Dormant until the player enters `triggerRadius`; on activation it
/// bursts `burstCount` Dregs, then (while the player stays inside the enlarged active area) emits
/// `sustainCount` Dregs every `sustainInterval` seconds. If the player leaves the active area for
/// `dormantDelay` seconds it goes dormant again and regenerates to full health. Destroyed for XP.
struct NodeSpawner {
    /// Dormant detection radius (player must enter this to wake the Node).
    float triggerRadius{350.0F};
    /// Active area once awake.
    float activeRadius{700.0F};
    /// While dormant, taking damage temporarily expands the trigger radius to this value.
    float damageTriggerRadius{700.0F};
    /// How long a hit keeps the trigger radius expanded (seconds).
    float damageTriggerDuration{6.0F};
    int burstCount{8};
    int sustainCount{3};
    float sustainInterval{3.0F};
    float dormantDelay{6.0F};
    /// Dreg burst speed buff (50% faster for 2s).
    float buffMultiplier{1.5F};
    float buffDuration{2.0F};

    bool active{false};
    float sustainTimer{0.0F};
    /// Counts up while the player is outside the active area; resets when inside.
    float dormantTimer{0.0F};
    /// Counts down while the damage-expanded trigger radius is in effect (dormant only).
    float damageTriggerTimer{0.0F};
    /// Last observed health, for detecting damage between ticks (-1 = uninitialized).
    float prevHealth{-1.0F};
};

/// Static Warden combat tuning from enemy archetype data (separate from runtime `WardenState`).
struct WardenTuning {
    float chaseSpeed{210.0F};
    float preferredRange{170.0F};
    float attackDamage{35.0F};
    float attackCooldown{1.6F};
    float attackTelegraph{0.5F};
    float attackRange{235.0F};
    float attackLineLength{245.0F};
    float attackLineHalfWidth{40.0F};
    float closeRange{95.0F};
    float abilityChargeTime{2.0F};
    float abilityCooldown{10.0F};
    float abilityKnockback{680.0F};
    float abilityKnockbackDuration{0.3F};
    float slowMultiplier{0.8F};
    float slowDuration{4.0F};
};

struct ManaShard {
    float manaAmount{20.0F};
};

/// Ground loot; `itemIndex` indexes into `InventoryState::items`.
struct ItemPickup {
    int itemIndex{-1};
};

/// Wall block in world space (`Transform::position` is center). `angle` (radians) rotates the box
/// about its center; 0 keeps it axis-aligned.
struct Wall {
    float halfW{32.0F};
    float halfH{32.0F};
    float angle{0.0F};
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

/// Stable classification for gameplay F-use handling (prefer over string `name`). `OldCasket`
/// covers every loot-casket tier (Old/Sealed/Wrought); the tier only affects the rolled contents
/// and the display `name`, not the F-use behavior.
enum class InteractableKind : uint8_t { Generic = 0, Anvil, OldCasket };

/// World interactable (casket, anvil, …); `name` shown in prompt.
struct Interactable {
    InteractableKind interactionKind{InteractableKind::Generic};
    std::string name{"Unknown"};
    bool opened{false};
};

/// Per-casket loot contents (item catalog ids; empty entries are skipped on open).
struct CasketLoot {
    std::array<std::string, 3> itemSlots{};
};

/// Active knockback: the entity skips its normal movement control and decelerates with friction
/// until elapsed >= duration. Used for both enemies (AI) and the player (input gating).
struct KnockbackState {
    float duration{0.25F};
    float elapsed{0.0F};
};

/// Per-Warden runtime state: telegraphed line slam + close-range push ability.
struct WardenState {
    float attackCooldownTimer{0.0F};
    bool telegraphActive{false};
    float telegraphTimer{0.0F};
    /// Direction frozen at slam commit; the line origin tracks the Warden's live position so the
    /// strike zone follows it (e.g. if it is knocked back mid-cast).
    Vector2 attackDir{1.0F, 0.0F};
    /// Brief countdown for the slam-landed flash VFX.
    float slamFlashTimer{0.0F};
    /// Close-range push ability.
    float abilityCooldownTimer{0.0F};
    float closeContactTimer{0.0F};
    float abilityFlashTimer{0.0F};
    /// One-shot SFX requests consumed (reset) by the gameplay scene.
    bool attackSfxPending{false};
    bool abilitySfxPending{false};
};

/// Timed movement slow on the player (multiplier < 1). Removed when elapsed >= duration.
struct PlayerSlow {
    float multiplier{0.8F};
    float duration{3.0F};
    float elapsed{0.0F};
};

/// Runtime payloads for item-driven timed effects (vials, Runic Shell cooldown, etc.).
struct ActiveItemEffectHot {
    float totalHeal{40.0F};
    float duration{8.0F};
    float elapsed{0.0F};
    float healedSoFar{0.0F};
    std::string sourceCatalogId{};
};

struct ActiveItemEffectManaRot {
    float totalMana{50.0F};
    float duration{6.0F};
    float elapsed{0.0F};
    float regenedSoFar{0.0F};
    std::string sourceCatalogId{};
};

struct ActiveItemEffectManic {
    float duration{7.0F};
    float elapsed{0.0F};
    float hpDrainTotal{0.0F};
    float hpDrained{0.0F};
    float speedMultiplier{2.0F};
    std::string sourceCatalogId{};
};

struct ActiveItemEffectRunicCd {
    float remaining{30.0F};
    float total{30.0F};
    std::string sourceCatalogId{};
};

using ActiveItemEffectEntry =
    std::variant<ActiveItemEffectHot, ActiveItemEffectManaRot, ActiveItemEffectManic,
                 ActiveItemEffectRunicCd>;

/// One player entity holds a vector of active item effect instances (replaces separate HoT/Manic
/// components).
struct ActiveItemEffects {
    std::vector<ActiveItemEffectEntry> entries{};
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
    float rangedProjectileSpeed{600.0F};
};

/// Movement + fog vision from character data (JSON).
struct PlayerMoveStats {
    float moveSpeed{dreadcast::config::PLAYER_MOVE_SPEED};
    float visionRange{dreadcast::config::FOG_OF_WAR_RADIUS};
};

/// Vigilant Eye dynamic vision modifier state.
struct VigilantEyeState {
    float currentRange{dreadcast::config::FOG_OF_WAR_RADIUS};
    float baseRange{dreadcast::config::FOG_OF_WAR_RADIUS};
    float stillSeconds{0.0F};
    bool stillBonusActive{false};
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
