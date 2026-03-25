#include "core/input.hpp"

#include <raylib.h>

namespace dreadcast {

void InputManager::beginFrame() {
    mouseDelta_ = GetMouseDelta();
    mousePos_ = GetMousePosition();
}

bool InputManager::isKeyHeld(int key) const { return IsKeyDown(key); }

bool InputManager::isKeyPressed(int key) const { return IsKeyPressed(key); }

bool InputManager::isKeyReleased(int key) const { return IsKeyReleased(key); }

bool InputManager::isMouseButtonHeld(int button) const {
    return IsMouseButtonDown(button);
}

bool InputManager::isMouseButtonPressed(int button) const {
    return IsMouseButtonPressed(button);
}

bool InputManager::isMouseButtonReleased(int button) const {
    return IsMouseButtonReleased(button);
}

} // namespace dreadcast
