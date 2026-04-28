#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dreadcast {

using SoundHandle = std::int32_t;

struct OutputDeviceInfo {
    std::string name;
    bool isSystemDefault{false};
    unsigned char deviceIdBytes[256]{};
};

class AudioSystem {
  public:
    AudioSystem();
    ~AudioSystem();

    AudioSystem(const AudioSystem &) = delete;
    AudioSystem &operator=(const AudioSystem &) = delete;

    void init(float masterVolume, float gameVolume, const std::string &preferredDeviceName);
    void shutdown();

    [[nodiscard]] bool isReady() const;

    void update();

    void setMasterVolume(float v);
    void setGameVolume(float v);
    [[nodiscard]] float masterVolume() const;
    [[nodiscard]] float gameVolume() const;

    [[nodiscard]] std::vector<OutputDeviceInfo> listOutputDevices() const;

    void applyOutputDeviceByName(const std::string &name);

    [[nodiscard]] const std::string &currentDeviceName() const;

    [[nodiscard]] SoundHandle loadSound(const std::string &resolvedPath);

    void playOneShot(SoundHandle handle, float pitch, float volumeMul = 1.0F);
    void playExclusive(SoundHandle handle, float pitch, float volumeMul = 1.0F);
    /// Starts looping playback on the exclusive slot if not already playing; always updates volume.
    void playExclusiveLoop(SoundHandle handle, float pitch, float volumeMul);
    void setExclusiveVolume(SoundHandle handle, float volumeMul);
    void stopExclusive(SoundHandle handle);
    [[nodiscard]] bool isExclusivePlaying(SoundHandle handle) const;
    void stopAll(SoundHandle handle);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dreadcast
