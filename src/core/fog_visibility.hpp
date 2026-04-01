#pragma once

#include <vector>

#include <entt/entt.hpp>
#include <raylib.h>

namespace dreadcast {

/// Fills `outWorldBoundary` with points on the vision perimeter in **world** space (angular
/// sweep). Vision is capped at `maxRadius` and blocked by axis-aligned `Wall` AABBs.
void buildVisibilityPolygonWorld(Vector2 playerWorld, entt::registry &registry, float maxRadius,
                                 std::vector<Vector2> &outWorldBoundary);

} // namespace dreadcast
