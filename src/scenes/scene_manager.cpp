#include "scenes/scene_manager.hpp"

#include "core/input.hpp"
#include "scenes/scene.hpp"

namespace dreadcast {

void SceneManager::replace(std::unique_ptr<Scene> scene) {
    if (!stack_.empty()) {
        stack_.back()->onExit();
        stack_.pop_back();
    }
    stack_.push_back(std::move(scene));
    stack_.back()->onEnter();
}

void SceneManager::push(std::unique_ptr<Scene> scene) {
    stack_.push_back(std::move(scene));
    stack_.back()->onEnter();
}

void SceneManager::pop() {
    if (stack_.empty()) {
        return;
    }
    stack_.back()->onExit();
    stack_.pop_back();
}

Scene *SceneManager::current() { return stack_.empty() ? nullptr : stack_.back().get(); }

const Scene *SceneManager::current() const {
    return stack_.empty() ? nullptr : stack_.back().get();
}

void SceneManager::update(InputManager &input, ResourceManager &resources, float frameDt) {
    if (!stack_.empty()) {
        stack_.back()->update(*this, input, resources, frameDt);
    }
}

void SceneManager::draw(ResourceManager &resources) {
    if (!stack_.empty()) {
        stack_.back()->draw(resources);
    }
}

} // namespace dreadcast
