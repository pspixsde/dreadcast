#pragma once

#include <string>
#include <unordered_map>

#include <raylib.h>

namespace dreadcast {

/// Runtime, user-tweakable settings shared across scenes.
struct GameSettings {
    bool showFpsCounter{true};
    /// Multiplier on aim vector from player screen position (0.1–3.0 typical).
    float mouseSensitivity{1.0F};
    /// HUD: mana cost on ability icons (Gameplay settings).
    bool showAbilityManaCost{true};
    /// Floating damage/heal numbers near entities.
    bool showDamageNumbers{false};
    /// Circular reload progress on the aim cursor while the chamber reloads.
    bool showReloadOnCursor{true};
    /// When bag is full, "Separate" still splits one unit by dropping it at the player.
    bool separateDropsWhenFull{false};

    [[nodiscard]] bool saveToFile(const std::string &path) const;
    /// Loads from `path` if present; ignores missing file / parse errors.
    void loadFromFile(const std::string &path);
};

/// Centralized texture, sound, and UI font loading.
class ResourceManager {
  public:
    ResourceManager();
    ~ResourceManager();

    Texture2D getTexture(const std::string &path);
    Sound getSound(const std::string &path);

    /// Loads the main UI font once. Falls back to the default font if loading fails.
    void loadUiFont(const std::string &path, int baseSize);

    const Font &uiFont() const { return uiFont_; }

    GameSettings &settings() { return settings_; }
    const GameSettings &settings() const { return settings_; }

    void unloadAll();

  private:
    std::unordered_map<std::string, Texture2D> textures_;
    std::unordered_map<std::string, Sound> sounds_;
    Font uiFont_{};
    bool uiFontLoaded_{false};
    bool uiFontOwned_{false};

    GameSettings settings_{};
};

} // namespace dreadcast
