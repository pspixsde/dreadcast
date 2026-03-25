#pragma once

#include <string>
#include <unordered_map>

#include <raylib.h>

namespace dreadcast {

/// Centralized texture, sound, and UI font loading.
class ResourceManager {
  public:
    ~ResourceManager();

    Texture2D getTexture(const std::string &path);
    Sound getSound(const std::string &path);

    /// Loads the main UI font once. Falls back to the default font if loading fails.
    void loadUiFont(const std::string &path, int baseSize);

    const Font &uiFont() const { return uiFont_; }

    void unloadAll();

  private:
    std::unordered_map<std::string, Texture2D> textures_;
    std::unordered_map<std::string, Sound> sounds_;
    Font uiFont_{};
    bool uiFontLoaded_{false};
    bool uiFontOwned_{false};
};

} // namespace dreadcast
