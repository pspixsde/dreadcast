#pragma once

namespace dreadcast {

class InputManager;
class ResourceManager;
class SceneManager;

class Scene {
  public:
    virtual ~Scene() = default;

    virtual void onEnter() {}
    virtual void onExit() {}

    virtual void update(SceneManager &scenes, InputManager &input, ResourceManager &resources,
                       float frameDt) = 0;
    virtual void draw(ResourceManager &resources) = 0;

    /// Draw software cursor on top of the scene (OS cursor is hidden globally).
    virtual void drawCursor(ResourceManager &resources);
};

} // namespace dreadcast
