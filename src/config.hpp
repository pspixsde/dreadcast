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

inline constexpr float MANA_COST_SHOT = 10.0F;
inline constexpr float PROJECTILE_DAMAGE = 10.0F;
inline constexpr float PROJECTILE_SPEED = 600.0F;
inline constexpr float PROJECTILE_MAX_RANGE = 800.0F;
inline constexpr float PROJECTILE_RADIUS = 5.0F;

inline constexpr float ENEMY_PROJECTILE_SPEED = 400.0F;
inline constexpr float ENEMY_PROJECTILE_MAX_RANGE = 400.0F;

inline constexpr float IMP_AGITATION_RANGE = 350.0F;
inline constexpr float ENEMY_CALM_DOWN_DELAY = 5.0F;

inline constexpr float PLAYER_BASE_MAX_HP = 100.0F;

inline constexpr float INTERACT_RANGE = 80.0F;

inline constexpr float HOT_DURATION = 8.0F;
inline constexpr float HOT_TOTAL_HEAL = 40.0F;

inline constexpr float MELEE_DAMAGE = 20.0F;
inline constexpr float MELEE_KNOCKBACK = 200.0F;
inline constexpr float MELEE_RANGE = 60.0F;
inline constexpr float MELEE_COOLDOWN = 0.4F;

inline constexpr float IMP_SHOOT_COOLDOWN = 2.0F;
inline constexpr float IMP_MIN_SHOOT_RANGE = 60.0F;

inline constexpr float MANA_SHARD_AMOUNT = 20.0F;

inline constexpr float ENEMY_VELOCITY_DAMPING = 0.88F;

/// Max distance from player (world units) to interact with ground loot.
inline constexpr float LOOT_PICKUP_RANGE = 80.0F;

} // namespace dreadcast::config
