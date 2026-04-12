#pragma once

#include <entt/entt.hpp>
#include <raylib.h>

namespace dreadcast {

struct InventoryState;

namespace ecs {

namespace collision {

/// Optional `snareImpactWorld` / `snareImpactFlashTimer` set when Deadlight Snare resolves (VFX).
void projectile_hits(entt::registry &registry, entt::entity playerEntity,
                     dreadcast::InventoryState *inventory, Vector2 *snareImpactWorld = nullptr,
                     float *snareImpactFlashTimer = nullptr);

void player_pickup_mana_shards(entt::registry &registry, entt::entity player);

/// Ground `ItemPickup` under `worldPoint` (world space), or `entt::null` if none.
entt::entity find_item_pickup_under_point(entt::registry &registry, Vector2 worldPoint);

/// Nearest `ItemPickup` within `maxRange` of `worldPos`, or `entt::null` if none.
entt::entity find_nearest_item_pickup_in_range(entt::registry &registry, Vector2 worldPos,
                                               float maxRange);

/// `ItemPickup` under `worldMouse` in world space if also within `maxRange` of `playerWorldPos`.
entt::entity find_item_pickup_hover_in_range(entt::registry &registry, Vector2 playerWorldPos,
                                             Vector2 worldMouse, float maxRange);

/// Interactable under mouse within range (not opened).
entt::entity find_interactable_hover_in_range(entt::registry &registry, Vector2 playerWorldPos,
                                              Vector2 worldMouse, float maxRange);

/// Adds loot to the first empty bag slot; destroys the pickup entity on success.
bool try_pickup_item_entity(entt::registry &registry, entt::entity pickupEntity,
                            InventoryState &inventory, float &inventoryFullFlashTimer);

/// After `InventoryState::removeItemAtIndex(removedIdx)` (swap-with-last), re-point ground
/// `ItemPickup::itemIndex` values that still referenced `oldLastIdx` to `removedIdx`.
void rewrite_ground_pickup_indices_after_remove(entt::registry &registry, int removedIdx,
                                                int oldLastIdx);

} // namespace collision
} // namespace ecs
} // namespace dreadcast
