#include "scenes/scene.hpp"

#include <raylib.h>

#include "core/cursor_draw.hpp"
#include "core/resource_manager.hpp"

namespace dreadcast {

void Scene::drawCursor(ResourceManager &resources) {
    drawCustomCursor(resources, CursorKind::Default, GetMousePosition());
}

} // namespace dreadcast
