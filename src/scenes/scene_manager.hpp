#pragma once

#include <memory>
#include <vector>

#include "scenes/scene.hpp"

namespace dreadcast {

class InputManager;
class ResourceManager;

class SceneManager {
  public:
    void replace(std::unique_ptr<Scene> scene);
    void push(std::unique_ptr<Scene> scene);
    void pop();

    Scene *current();
    const Scene *current() const;

    bool empty() const { return stack_.empty(); }

    void requestQuit() { quitRequested_ = true; }
    bool shouldQuit() const { return quitRequested_; }

    void update(InputManager &input, ResourceManager &resources, float frameDt);
    void draw(ResourceManager &resources);
    void drawCursor(ResourceManager &resources);

  private:
    std::vector<std::unique_ptr<Scene>> stack_;
    bool quitRequested_{false};
};

} // namespace dreadcast
