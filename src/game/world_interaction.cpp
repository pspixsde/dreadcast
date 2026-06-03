#include "game/world_interaction.hpp"

#include <cmath>

#include "ecs/components.hpp"
#include "ecs/systems/collision_system.hpp"
#include "game/game_data.hpp"
#include "game/item_transaction.hpp"
#include "game/items.hpp"

namespace dreadcast {

WorldInteractDispatch dispatchInteractablePrimaryUse(ecs::InteractableKind kind, bool &interactionOpenedMarker,
                                                      const WorldInteractEnv &env) {
    switch (kind) {
    case ecs::InteractableKind::OldCasket:
        if (interactionOpenedMarker) {
            return WorldInteractDispatch::None;
        }
        if (env.registry == nullptr || env.invCtx == nullptr || env.casketTf == nullptr ||
            env.casketSpr == nullptr || env.casketItemSlotBegin == nullptr ||
            env.casketItemSlotCount == 0) {
            return WorldInteractDispatch::None;
        }
        interactionOpenedMarker = true;
        spawnCasketLootPickupRing(*env.registry, *env.invCtx, *env.casketTf, *env.casketSpr, env.playerPos,
                                  env.casketItemSlotBegin, env.casketItemSlotCount);
        return WorldInteractDispatch::OpenedLootCasket;
    case ecs::InteractableKind::Anvil:
        return WorldInteractDispatch::RequestOpenAnvil;
    default:
        return WorldInteractDispatch::None;
    }
}

void spawnCasketLootPickupRing(entt::registry &registry, InvCtx &ctx, const ecs::Transform &casketTf,
                               const ecs::Sprite &casketSpr, const Vector2 playerPos,
                               const std::string *itemSlotBegin, std::size_t itemSlotCount) {
    Vector2 dir = {playerPos.x - casketTf.position.x, playerPos.y - casketTf.position.y};
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
    constexpr float spread = 14.0F;

    int dropIdx = 0;
    for (std::size_t i = 0; i < itemSlotCount; ++i) {
        const std::string &kind = itemSlotBegin[i];
        if (kind.empty() || kind == "-") {
            continue;
        }
        ItemData it = makeItemFromMapKind(kind);
        if (it.name.empty()) {
            continue;
        }
        const int idx = allocatePickupPoolItem(ctx, std::move(it));
        if (idx < 0) {
            continue;
        }
        const float ang = static_cast<float>(dropIdx) * 0.85F;
        const Vector2 drop = {
            casketTf.position.x + dir.x * baseDist + perp.x * spread * std::cosf(ang) +
                static_cast<float>(dropIdx) * 6.0F * dir.x,
            casketTf.position.y + dir.y * baseDist + perp.y * spread * std::cosf(ang) +
                static_cast<float>(dropIdx) * 6.0F * dir.y};
        ecs::collision::spawn_item_pickup_at_world(registry, drop, idx);
        ++dropIdx;
    }
}

} // namespace dreadcast
