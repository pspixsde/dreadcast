#pragma once

#include <entt/entt.hpp>
#include <raylib.h>

namespace dreadcast {
struct EnemyArchetype;
}

namespace dreadcast::ecs {

/// Creates a fully-wired enemy entity (Transform, Velocity, Sprite, Facing, Health, Enemy,
/// NameTag, EnemyAI [+ WardenTuning/State for bruisers], Agitation, XP/level) from a catalog
/// archetype at `pos`. Shared by map spawning and runtime spawners (Nodes).
entt::entity spawnEnemyFromArchetype(entt::registry &registry, const EnemyArchetype &arch,
                                     Vector2 pos);

} // namespace dreadcast::ecs
