#pragma once

#include <string>
#include <unordered_map>

#include <raylib.h>

#include "core/audio.hpp"

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
    /// Master volume (0–1) applied to the audio engine.
    float masterVolume{1.0F};
    /// Game / SFX volume (0–1), multiplied with master for gameplay sounds.
    float gameVolume{1.0F};
    /// Display name from audio device list, or empty / "System Default".
    std::string audioDeviceName{};

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
    /// Cached miniaudio handle; -1 if load failed.
    [[nodiscard]] SoundHandle getSound(const std::string &path);

    AudioSystem &audio() { return audio_; }
    const AudioSystem &audio() const { return audio_; }

    /// Call once per frame (decodes finished one-shot instances).
    void updateAudio();

    /// Rebuilds the audio backend from current `settings_` (used after Reset / device change).
    void reinitAudioFromSettings();

    /// Loads the main UI font once. Falls back to the default font if loading fails.
    void loadUiFont(const std::string &path, int baseSize);

    const Font &uiFont() const { return uiFont_; }

    GameSettings &settings() { return settings_; }
    const GameSettings &settings() const { return settings_; }

    void unloadAll();

  private:
    std::unordered_map<std::string, Texture2D> textures_;
    std::unordered_map<std::string, SoundHandle> soundHandles_;

    AudioSystem audio_{};

    Font uiFont_{};
    bool uiFontLoaded_{false};
    bool uiFontOwned_{false};

    GameSettings settings_{};
};

} // namespace dreadcast
