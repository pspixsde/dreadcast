#pragma once

namespace dreadcast::config {

inline constexpr int WINDOW_WIDTH = 1920;
inline constexpr int WINDOW_HEIGHT = 1080;
inline constexpr const char *WINDOW_TITLE = "Dreadcast";

/// Target render FPS (vsync / frame pacing).
inline constexpr int TARGET_FPS = 60;

/// Fixed simulation ticks per second (deterministic physics / combat).
inline constexpr int TICKS_PER_SECOND = 60;
inline constexpr float FIXED_DT = 1.0F / static_cast<float>(TICKS_PER_SECOND);

/// Player movement speed (world units per second).
inline constexpr float PLAYER_MOVE_SPEED = 280.0F;

/// Camera follow smoothing (higher = snappier).
inline constexpr float CAMERA_FOLLOW_LERP = 8.0F;

/// UI font (Cinzel variable font from Google Fonts).
inline constexpr const char *UI_FONT_PATH = "assets/fonts/Cinzel.ttf";
inline constexpr int UI_FONT_BASE_SIZE = 96;

/// Software cursor: 128×128 source art is scaled by this for on-screen size (hotspot scales too).
/// 0.25 → 4× smaller than full size (~32 px); tune if needed.
inline constexpr float CURSOR_DISPLAY_SCALE = 0.35F;

inline constexpr float PROJECTILE_DAMAGE = 15.0F;
inline constexpr int CHAMBER_MAX_SHOTS = 6;
inline constexpr float CHAMBER_RELOAD_TIME = 2.0F;
inline constexpr float CHAMBER_IDLE_RELOAD_TIME = 3.0F;
inline constexpr float PROJECTILE_SPEED = 600.0F;
inline constexpr float PROJECTILE_MAX_RANGE = 800.0F;
inline constexpr float PROJECTILE_RADIUS = 5.0F;

inline constexpr float ENEMY_PROJECTILE_SPEED = 400.0F;
inline constexpr float ENEMY_PROJECTILE_MAX_RANGE = 400.0F;

inline constexpr float IMP_AGITATION_RANGE = 350.0F;
inline constexpr float ENEMY_CALM_DOWN_DELAY = 5.0F;
inline constexpr float IMP_SPRITE_SIZE = 40.0F;
inline constexpr float IMP_PREFERRED_RANGE = 220.0F;
inline constexpr float IMP_KITE_SPEED = 145.0F;
inline constexpr float IMP_ADVANCE_SPEED = 100.0F;
inline constexpr float IMP_STRAFE_BIAS = 0.4F;
/// Below this distance Imps kite harder to avoid melee; above it they prioritize chasing in the
/// outer preferred band.
inline constexpr float IMP_PANIC_RANGE = 80.0F;

inline constexpr float HELLHOUND_HP = 60.0F;
inline constexpr float HELLHOUND_DAMAGE = 15.0F;
inline constexpr float HELLHOUND_SPRITE_SIZE = 48.0F;
inline constexpr float HELLHOUND_CHASE_SPEED = 210.0F;
inline constexpr float HELLHOUND_MELEE_RANGE = 44.0F;
inline constexpr float HELLHOUND_MELEE_COOLDOWN = 1.0F;
inline constexpr float HELLHOUND_AGITATION_RANGE = 420.0F;

inline constexpr float PLAYER_BASE_MAX_HP = 100.0F;

inline constexpr float INTERACT_RANGE = 80.0F;

inline constexpr float HOT_DURATION = 8.0F;
inline constexpr float HOT_TOTAL_HEAL = 40.0F;

inline constexpr float MANIC_DURATION = 7.0F;
inline constexpr float MANIC_HP_DRAIN_PERCENT = 0.40F;
inline constexpr float MANIC_SPEED_MULTIPLIER = 2.0F;
inline constexpr float MANIC_MIN_HP_FRACTION = 0.40F;

inline constexpr float MELEE_DAMAGE = 20.0F;
/// Base speed applied to enemies on RMB melee hits (scaled per combo swing).
inline constexpr float MELEE_KNOCKBACK = 400.0F;
inline constexpr float MELEE_RANGE = 60.0F;
inline constexpr float MELEE_COOLDOWN = 0.4F;
inline constexpr float KNOCKBACK_DURATION = 0.25F;
inline constexpr float KNOCKBACK_FRICTION = 0.90F;

inline constexpr float IMP_SHOOT_COOLDOWN = 2.0F;
inline constexpr float IMP_MIN_SHOOT_RANGE = 60.0F;

inline constexpr float MANA_SHARD_AMOUNT = 20.0F;

/// Enemy steering: lerp current velocity toward AI desired velocity (per second scale, clamped).
inline constexpr float ENEMY_STEER_RATE = 12.0F;
/// When seeking last known player position, stop searching inside this radius (world units).
inline constexpr float ENEMY_SEEK_ARRIVE_RADIUS = 22.0F;
/// Time before enemy is considered stuck against a wall (seconds).
inline constexpr float ENEMY_STUCK_THRESHOLD = 0.25F;
/// Minimum displacement per tick to not be considered stuck (world units).
inline constexpr float ENEMY_STUCK_MIN_DISP = 1.5F;

/// Max distance from player (world units) to interact with ground loot.
inline constexpr float LOOT_PICKUP_RANGE = 80.0F;

inline constexpr float RUNIC_SHELL_HP_THRESHOLD = 0.30F;
inline constexpr float RUNIC_SHELL_DAMAGE = 30.0F;
inline constexpr float RUNIC_SHELL_HEAL = 30.0F;
inline constexpr float RUNIC_SHELL_KNOCKBACK = 500.0F;
inline constexpr float RUNIC_SHELL_RADIUS = 180.0F;
inline constexpr float RUNIC_SHELL_COOLDOWN = 30.0F;

inline constexpr float FOG_OF_WAR_RADIUS = 500.0F;
inline constexpr unsigned char FOG_DARKNESS_ALPHA = 145;
/// Screen-space half-width of the fog transition at the visibility boundary (mask blur). 0 = hard
/// edge; ~12–18 reads closer to Dota-style soft fog.
inline constexpr float FOG_EDGE_SOFTEN_PIXELS = 17.0F;

/// Lava: movement multiplier while standing in lava (0.5 = half speed = 2x slower).
inline constexpr float LAVA_SPEED_MULTIPLIER = 0.5F;
inline constexpr float LAVA_DAMAGE_PER_TICK = 8.0F;
inline constexpr float LAVA_DAMAGE_INTERVAL = 0.5F;

/// Vial of Raw Spirit (consumable)
inline constexpr float RAW_SPIRIT_MANA_TOTAL = 50.0F;
inline constexpr float RAW_SPIRIT_DURATION = 6.0F;

/// Angular samples for world-space vision polygon (ray cast per direction).
inline constexpr int FOG_VISIBILITY_SAMPLES = 360;

} // namespace dreadcast::config
