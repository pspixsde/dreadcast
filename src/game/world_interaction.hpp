#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

#include <entt/fwd.hpp>
#include <raylib.h>

namespace dreadcast {

struct InvCtx;

namespace ecs {
enum class InteractableKind : std::uint8_t;
struct Interactable;
struct Sprite;
struct Transform;
} // namespace ecs

enum class WorldInteractDispatch : std::uint8_t { None, OpenedLootCasket, RequestOpenAnvil };

/// Inputs for F-key interaction on a hovered `Interactable` (casket loot needs map data + ECS).
struct WorldInteractEnv {
    entt::registry *registry{nullptr};
    InvCtx *invCtx{nullptr};
    Vector2 playerPos{};
    ecs::Transform *casketTf{nullptr};
    ecs::Sprite *casketSpr{nullptr};
    const std::string *casketItemSlotBegin{nullptr};
    std::size_t casketItemSlotCount{0};
};

WorldInteractDispatch dispatchInteractablePrimaryUse(ecs::InteractableKind kind, bool &interactionOpenedMarker,
                                                      const WorldInteractEnv &env);

/// Pool-only allocations via `allocatePickupPoolItem` → radial world pickups for casket contents.
void spawnCasketLootPickupRing(entt::registry &registry, InvCtx &ctx, const ecs::Transform &casketTf,
                               const ecs::Sprite &casketSpr, Vector2 playerPos,
                               const std::string *itemSlotBegin, std::size_t itemSlotCount);

} // namespace dreadcast
