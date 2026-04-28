#include "core/audio.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <memory>
#include <unordered_map>

#define MINIAUDIO_IMPLEMENTATION
// Ensure OGG/Vorbis decoding when loading .ogg ambience assets.
#define MA_ENABLE_VORBIS
#define MA_ENABLE_STB_VORBIS
#include <miniaudio.h>

namespace dreadcast {

namespace {

[[nodiscard]] bool endsWithIgnoreCase(const std::string &s, const char *suffix) {
    const size_t sl = s.size();
    const size_t tl = std::strlen(suffix);
    if (sl < tl) {
        return false;
    }
    const size_t off = sl - tl;
    for (size_t i = 0; i < tl; ++i) {
        const unsigned char a = static_cast<unsigned char>(s[off + i]);
        const unsigned char b = static_cast<unsigned char>(suffix[i]);
        if (std::tolower(a) != std::tolower(b)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] ma_uint32 soundFlagsForPath(const std::string &path) {
    // Stream compressed ambience/music-like assets (not fully decoded upfront).
    if (endsWithIgnoreCase(path, ".ogg")) {
        return MA_SOUND_FLAG_STREAM;
    }
    return 0;
}

[[nodiscard]] ma_result initSoundFromFileWithFallback(ma_engine *engine, const std::string &path,
                                                      ma_uint32 preferredFlags,
                                                      ma_sound *outSound) {
    if (engine == nullptr || outSound == nullptr) {
        return MA_INVALID_ARGS;
    }
    const ma_uint32 attempts[] = {
        preferredFlags,
        0,
        MA_SOUND_FLAG_DECODE,
    };
    ma_result last = MA_ERROR;
    ma_uint32 prev = 0xFFFFFFFFu;
    for (ma_uint32 flags : attempts) {
        if (flags == prev) {
            continue;
        }
        prev = flags;
        last = ma_sound_init_from_file(engine, path.c_str(), flags, nullptr, nullptr, outSound);
        if (last == MA_SUCCESS) {
            return MA_SUCCESS;
        }
    }
    return last;
}

void deviceIdToBytes(const ma_device_id &id, unsigned char *out, size_t cap) {
    const size_t n = sizeof(ma_device_id);
    if (cap < n) {
        return;
    }
    std::memcpy(out, &id, n);
}

} // namespace

struct AudioSystem::Impl {
    bool contextReady{false};
    bool engineReady{false};

    float masterVolume{1.0F};
    float gameVolume{1.0F};
    std::string currentDeviceName{"System Default"};

    ma_context context{};
    ma_engine engine{};

    /// Heap-allocated: `ma_sound` must never be moved (miniaudio holds internal pointers to it).
    struct TemplateSlot {
        std::string path;
        std::unique_ptr<ma_sound> sound{};
        bool loaded{false};
    };

    struct LiveCopy {
        SoundHandle handle{-1};
        std::unique_ptr<ma_sound> sound{};
    };

    std::vector<TemplateSlot> templates;
    std::unordered_map<std::string, SoundHandle> pathToHandle;
    std::vector<LiveCopy> liveCopies;
    std::unordered_map<SoundHandle, std::unique_ptr<ma_sound>> exclusiveByHandle;

    void tearDownSounds() {
        for (LiveCopy &lc : liveCopies) {
            if (lc.sound) {
                ma_sound_uninit(lc.sound.get());
                lc.sound.reset();
            }
        }
        liveCopies.clear();

        for (auto &kv : exclusiveByHandle) {
            if (kv.second) {
                ma_sound_uninit(kv.second.get());
                kv.second.reset();
            }
        }
        exclusiveByHandle.clear();

        for (TemplateSlot &t : templates) {
            if (t.loaded && t.sound) {
                ma_sound_uninit(t.sound.get());
                t.sound.reset();
                t.loaded = false;
            }
        }
    }

    bool startEngine(const std::string &deviceName) {
        if (engineReady) {
            ma_engine_uninit(&engine);
            engineReady = false;
        }

        ma_engine_config cfg = ma_engine_config_init();
        cfg.pContext = &context;

        ma_device_id chosen{};
        ma_device_id *pChosen = nullptr;
        if (deviceName.empty() || deviceName == "System Default") {
            pChosen = nullptr;
            currentDeviceName = "System Default";
        } else {
            ma_device_info *pPlayback = nullptr;
            ma_uint32 playbackCount = 0;
            bool found = false;
            if (ma_context_get_devices(&context, &pPlayback, &playbackCount, nullptr, nullptr) ==
                    MA_SUCCESS &&
                pPlayback != nullptr) {
                for (ma_uint32 i = 0; i < playbackCount; ++i) {
                    if (deviceName == pPlayback[i].name) {
                        chosen = pPlayback[i].id;
                        pChosen = &chosen;
                        currentDeviceName = deviceName;
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                std::fprintf(stderr, "Dreadcast: audio device \"%s\" not found, using default\n",
                             deviceName.c_str());
                pChosen = nullptr;
                currentDeviceName = "System Default";
            }
        }

        cfg.pPlaybackDeviceID = pChosen;

        if (ma_engine_init(&cfg, &engine) != MA_SUCCESS) {
            std::fprintf(stderr, "Dreadcast: ma_engine_init failed\n");
            return false;
        }
        engineReady = true;
        ma_engine_set_volume(&engine, masterVolume);
        return true;
    }

    void reloadTemplatesAfterEngineRestart() {
        for (TemplateSlot &t : templates) {
            if (t.path.empty()) {
                continue;
            }
            if (!t.sound) {
                t.sound = std::make_unique<ma_sound>();
            }
            const ma_uint32 flags = soundFlagsForPath(t.path);
            const ma_result rc =
                initSoundFromFileWithFallback(&engine, t.path, flags, t.sound.get());
            if (rc != MA_SUCCESS) {
                t.loaded = false;
                std::fprintf(stderr, "Dreadcast: could not reload sound \"%s\" (ma_result=%d)\n",
                             t.path.c_str(), static_cast<int>(rc));
            } else {
                t.loaded = true;
            }
        }
    }
};

AudioSystem::AudioSystem() : impl_(std::make_unique<Impl>()) {}

AudioSystem::~AudioSystem() { shutdown(); }

void AudioSystem::init(float masterVolume, float gameVolume,
                       const std::string &preferredDeviceName) {
    impl_->masterVolume = std::clamp(masterVolume, 0.0F, 1.0F);
    impl_->gameVolume = std::clamp(gameVolume, 0.0F, 1.0F);

    if (!impl_->contextReady) {
        if (ma_context_init(nullptr, 0, nullptr, &impl_->context) != MA_SUCCESS) {
            std::fprintf(stderr, "Dreadcast: ma_context_init failed\n");
            return;
        }
        impl_->contextReady = true;
    }

    impl_->currentDeviceName =
        preferredDeviceName.empty() ? "System Default" : preferredDeviceName;
    if (!impl_->startEngine(impl_->currentDeviceName)) {
        return;
    }

    ma_engine_set_volume(&impl_->engine, impl_->masterVolume);
    impl_->reloadTemplatesAfterEngineRestart();
}

void AudioSystem::shutdown() {
    if (!impl_) {
        return;
    }
    impl_->tearDownSounds();
    if (impl_->engineReady) {
        ma_engine_uninit(&impl_->engine);
        impl_->engineReady = false;
    }
    if (impl_->contextReady) {
        ma_context_uninit(&impl_->context);
        impl_->contextReady = false;
    }
}

bool AudioSystem::isReady() const { return impl_ && impl_->engineReady; }

void AudioSystem::update() {
    if (!impl_ || !impl_->engineReady) {
        return;
    }
    for (auto it = impl_->liveCopies.begin(); it != impl_->liveCopies.end();) {
        if (it->sound && ma_sound_at_end(it->sound.get())) {
            ma_sound_uninit(it->sound.get());
            it->sound.reset();
            it = impl_->liveCopies.erase(it);
        } else {
            ++it;
        }
    }
}

void AudioSystem::setMasterVolume(float v) {
    if (!impl_) {
        return;
    }
    impl_->masterVolume = std::clamp(v, 0.0F, 1.0F);
    if (impl_->engineReady) {
        ma_engine_set_volume(&impl_->engine, impl_->masterVolume);
    }
}

void AudioSystem::setGameVolume(float v) {
    if (!impl_) {
        return;
    }
    impl_->gameVolume = std::clamp(v, 0.0F, 1.0F);
}

float AudioSystem::masterVolume() const { return impl_ ? impl_->masterVolume : 1.0F; }

float AudioSystem::gameVolume() const { return impl_ ? impl_->gameVolume : 1.0F; }

std::vector<OutputDeviceInfo> AudioSystem::listOutputDevices() const {
    std::vector<OutputDeviceInfo> out;
    OutputDeviceInfo def;
    def.name = "System Default";
    def.isSystemDefault = true;
    ma_device_id z{};
    deviceIdToBytes(z, def.deviceIdBytes, sizeof(def.deviceIdBytes));
    out.push_back(def);

    if (!impl_ || !impl_->contextReady) {
        return out;
    }

    ma_device_info *pPlayback = nullptr;
    ma_uint32 playbackCount = 0;
    if (ma_context_get_devices(&impl_->context, &pPlayback, &playbackCount, nullptr, nullptr) !=
            MA_SUCCESS ||
        pPlayback == nullptr) {
        return out;
    }

    for (ma_uint32 i = 0; i < playbackCount; ++i) {
        OutputDeviceInfo info;
        info.name = pPlayback[i].name;
        info.isSystemDefault = false;
        deviceIdToBytes(pPlayback[i].id, info.deviceIdBytes, sizeof(info.deviceIdBytes));
        out.push_back(info);
    }
    return out;
}

void AudioSystem::applyOutputDeviceByName(const std::string &name) {
    if (!impl_) {
        return;
    }
    impl_->tearDownSounds();
    impl_->currentDeviceName = name.empty() ? "System Default" : name;
    if (!impl_->startEngine(impl_->currentDeviceName)) {
        return;
    }
    ma_engine_set_volume(&impl_->engine, impl_->masterVolume);
    impl_->reloadTemplatesAfterEngineRestart();
}

const std::string &AudioSystem::currentDeviceName() const {
    static const std::string fallback{"System Default"};
    if (!impl_) {
        return fallback;
    }
    return impl_->currentDeviceName;
}

SoundHandle AudioSystem::loadSound(const std::string &resolvedPath) {
    if (!impl_ || !impl_->engineReady) {
        return -1;
    }
    const auto it = impl_->pathToHandle.find(resolvedPath);
    if (it != impl_->pathToHandle.end()) {
        return it->second;
    }

    const SoundHandle h = static_cast<SoundHandle>(impl_->templates.size());
    impl_->templates.push_back({});
    {
        auto &slot = impl_->templates.back();
        slot.path = resolvedPath;
        slot.sound = std::make_unique<ma_sound>();
        const ma_uint32 flags = soundFlagsForPath(resolvedPath);
        const ma_result rc =
            initSoundFromFileWithFallback(&impl_->engine, resolvedPath, flags, slot.sound.get());
        if (rc != MA_SUCCESS) {
            impl_->templates.pop_back();
            std::fprintf(stderr, "Dreadcast: could not load sound \"%s\" (ma_result=%d)\n",
                         resolvedPath.c_str(), static_cast<int>(rc));
            return -1;
        }
        slot.loaded = true;
    }
    impl_->pathToHandle[resolvedPath] = h;
    return h;
}

void AudioSystem::playOneShot(SoundHandle handle, float pitch, float volumeMul) {
    if (!impl_ || !impl_->engineReady || handle < 0 ||
        handle >= static_cast<SoundHandle>(impl_->templates.size()) ||
        !impl_->templates[static_cast<size_t>(handle)].loaded ||
        !impl_->templates[static_cast<size_t>(handle)].sound) {
        return;
    }
    Impl::LiveCopy lc;
    lc.handle = handle;
    lc.sound = std::make_unique<ma_sound>();
    if (ma_sound_init_copy(&impl_->engine,
                           impl_->templates[static_cast<size_t>(handle)].sound.get(), 0, nullptr,
                           lc.sound.get()) != MA_SUCCESS) {
        lc.sound.reset();
        return;
    }
    ma_sound_set_pitch(lc.sound.get(), pitch);
    ma_sound_set_volume(lc.sound.get(), volumeMul * impl_->gameVolume);
    ma_sound_start(lc.sound.get());
    impl_->liveCopies.push_back(std::move(lc));
}

void AudioSystem::playExclusive(SoundHandle handle, float pitch, float volumeMul) {
    if (!impl_ || !impl_->engineReady || handle < 0 ||
        handle >= static_cast<SoundHandle>(impl_->templates.size()) ||
        !impl_->templates[static_cast<size_t>(handle)].loaded ||
        !impl_->templates[static_cast<size_t>(handle)].sound) {
        return;
    }

    auto it = impl_->exclusiveByHandle.find(handle);
    if (it == impl_->exclusiveByHandle.end() || !it->second) {
        auto p = std::make_unique<ma_sound>();
        const std::string &path = impl_->templates[static_cast<size_t>(handle)].path;
        const ma_uint32 flags = soundFlagsForPath(path);
        const ma_result rc =
            (flags == 0)
                ? ma_sound_init_copy(&impl_->engine,
                                     impl_->templates[static_cast<size_t>(handle)].sound.get(), 0,
                                     nullptr, p.get())
                : initSoundFromFileWithFallback(&impl_->engine, path, flags, p.get());
        if (rc != MA_SUCCESS) {
            std::fprintf(stderr, "Dreadcast: could not create looping sound instance for \"%s\"\n",
                         path.c_str());
            return;
        }
        auto ins = impl_->exclusiveByHandle.emplace(handle, std::move(p));
        it = ins.first;
    }

    ma_sound *s = it->second.get();
    ma_sound_stop(s);
    ma_sound_set_looping(s, MA_FALSE);
    ma_sound_set_pitch(s, pitch);
    ma_sound_set_volume(s, volumeMul * impl_->gameVolume);
    ma_sound_seek_to_pcm_frame(s, 0);
    ma_sound_start(s);
}

void AudioSystem::playExclusiveLoop(SoundHandle handle, float pitch, float volumeMul) {
    if (!impl_ || !impl_->engineReady || handle < 0 ||
        handle >= static_cast<SoundHandle>(impl_->templates.size()) ||
        !impl_->templates[static_cast<size_t>(handle)].loaded ||
        !impl_->templates[static_cast<size_t>(handle)].sound) {
        return;
    }

    auto it = impl_->exclusiveByHandle.find(handle);
    if (it == impl_->exclusiveByHandle.end() || !it->second) {
        auto p = std::make_unique<ma_sound>();
        if (ma_sound_init_copy(&impl_->engine,
                               impl_->templates[static_cast<size_t>(handle)].sound.get(), 0,
                               nullptr, p.get()) != MA_SUCCESS) {
            return;
        }
        auto ins = impl_->exclusiveByHandle.emplace(handle, std::move(p));
        it = ins.first;
    }

    ma_sound *s = it->second.get();
    ma_sound_set_looping(s, MA_TRUE);
    ma_sound_set_pitch(s, pitch);
    ma_sound_set_volume(s, volumeMul * impl_->gameVolume);
    if (!ma_sound_is_playing(s)) {
        ma_sound_stop(s);
        ma_sound_seek_to_pcm_frame(s, 0);
        ma_sound_start(s);
    }
}

void AudioSystem::setExclusiveVolume(SoundHandle handle, float volumeMul) {
    if (!impl_ || !impl_->engineReady) {
        return;
    }
    const auto it = impl_->exclusiveByHandle.find(handle);
    if (it == impl_->exclusiveByHandle.end() || !it->second) {
        return;
    }
    ma_sound_set_volume(it->second.get(), volumeMul * impl_->gameVolume);
}

void AudioSystem::stopExclusive(SoundHandle handle) {
    if (!impl_) {
        return;
    }
    const auto it = impl_->exclusiveByHandle.find(handle);
    if (it == impl_->exclusiveByHandle.end() || !it->second) {
        return;
    }
    ma_sound *s = it->second.get();
    ma_sound_stop(s);
    ma_sound_seek_to_pcm_frame(s, 0);
}

bool AudioSystem::isExclusivePlaying(SoundHandle handle) const {
    if (!impl_) {
        return false;
    }
    const auto it = impl_->exclusiveByHandle.find(handle);
    if (it == impl_->exclusiveByHandle.end() || !it->second) {
        return false;
    }
    return ma_sound_is_playing(it->second.get()) != 0;
}

void AudioSystem::stopAll(SoundHandle handle) {
    if (!impl_) {
        return;
    }
    for (auto it = impl_->liveCopies.begin(); it != impl_->liveCopies.end();) {
        if (it->handle == handle && it->sound) {
            ma_sound_stop(it->sound.get());
            ma_sound_uninit(it->sound.get());
            it->sound.reset();
            it = impl_->liveCopies.erase(it);
        } else {
            ++it;
        }
    }
    const auto ex = impl_->exclusiveByHandle.find(handle);
    if (ex != impl_->exclusiveByHandle.end() && ex->second) {
        ma_sound_stop(ex->second.get());
    }
}

} // namespace dreadcast
