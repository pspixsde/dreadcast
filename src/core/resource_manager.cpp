#include "core/resource_manager.hpp"

#include <raylib.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "game/game_data.hpp"

namespace dreadcast {

namespace {

std::string joinPath(const std::string &base, const std::string &relative) {
    if (base.empty()) {
        return relative;
    }
    const char last = base.back();
    const bool hasSep = (last == '/' || last == '\\');
    return hasSep ? (base + relative) : (base + "/" + relative);
}

/// Try cwd-relative path, then next to the executable (CMake POST_BUILD copies `assets/` there).
std::string resolveAssetPath(const std::string &relativePath) {
    if (FileExists(relativePath.c_str())) {
        return relativePath;
    }
    const std::string nearExe = joinPath(GetApplicationDirectory(), relativePath);
    if (FileExists(nearExe.c_str())) {
        return nearExe;
    }
    return relativePath;
}

} // namespace

bool GameSettings::saveToFile(const std::string &path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out << "mouseSensitivity=" << static_cast<double>(mouseSensitivity) << '\n';
    out << "showFpsCounter=" << (showFpsCounter ? 1 : 0) << '\n';
    out << "showAbilityManaCost=" << (showAbilityManaCost ? 1 : 0) << '\n';
    out << "showDamageNumbers=" << (showDamageNumbers ? 1 : 0) << '\n';
    out << "showReloadOnCursor=" << (showReloadOnCursor ? 1 : 0) << '\n';
    out << "separateDropsWhenFull=" << (separateDropsWhenFull ? 1 : 0) << '\n';
    out << "bagPriorityShiftIntoInventory=" << (bagPriorityShiftIntoInventory ? 1 : 0) << '\n';
    out << "masterVolume=" << static_cast<double>(masterVolume) << '\n';
    out << "gameVolume=" << static_cast<double>(gameVolume) << '\n';
    out << "audioDeviceName=" << audioDeviceName << '\n';
    return out.good();
}

void GameSettings::loadFromFile(const std::string &path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, eq);
        const std::string val = line.substr(eq + 1);
        std::istringstream vs(val);
        if (key == "mouseSensitivity") {
            double d = 0.0;
            if (vs >> d) {
                mouseSensitivity = static_cast<float>(d);
            }
        } else if (key == "showFpsCounter") {
            int i = 0;
            if (vs >> i) {
                showFpsCounter = (i != 0);
            }
        } else if (key == "showAbilityManaCost") {
            int i = 0;
            if (vs >> i) {
                showAbilityManaCost = (i != 0);
            }
        } else if (key == "showDamageNumbers") {
            int i = 0;
            if (vs >> i) {
                showDamageNumbers = (i != 0);
            }
        } else if (key == "showReloadOnCursor") {
            int i = 0;
            if (vs >> i) {
                showReloadOnCursor = (i != 0);
            }
        } else if (key == "separateDropsWhenFull") {
            int i = 0;
            if (vs >> i) {
                separateDropsWhenFull = (i != 0);
            }
        } else if (key == "bagPriorityShiftIntoInventory") {
            int i = 0;
            if (vs >> i) {
                bagPriorityShiftIntoInventory = (i != 0);
            }
        } else if (key == "masterVolume") {
            double d = 0.0;
            if (vs >> d) {
                masterVolume = static_cast<float>(d);
            }
        } else if (key == "gameVolume") {
            double d = 0.0;
            if (vs >> d) {
                gameVolume = static_cast<float>(d);
            }
        } else if (key == "audioDeviceName") {
            audioDeviceName = val;
        }
    }
}

ResourceManager::ResourceManager() {
    settings_.loadFromFile("settings.cfg");
    audio_.init(settings_.masterVolume, settings_.gameVolume, settings_.audioDeviceName);
    if (audio_.isReady()) {
        settings_.audioDeviceName = audio_.currentDeviceName();
    }
    static_cast<void>(loadGameData());
}

ResourceManager::~ResourceManager() { unloadAll(); }

void ResourceManager::updateAudio() { audio_.update(); }

void ResourceManager::reinitAudioFromSettings() {
    soundHandles_.clear();
    audio_.shutdown();
    audio_.init(settings_.masterVolume, settings_.gameVolume, settings_.audioDeviceName);
    if (audio_.isReady()) {
        settings_.audioDeviceName = audio_.currentDeviceName();
    }
}

Texture2D ResourceManager::getTexture(const std::string &path) {
    auto it = textures_.find(path);
    if (it != textures_.end()) {
        return it->second;
    }
    const std::string resolved = resolveAssetPath(path);
    const Texture2D tex = LoadTexture(resolved.c_str());
    if (tex.id != 0) {
        SetTextureFilter(tex, TEXTURE_FILTER_POINT);
    } else {
        TraceLog(LOG_WARNING, "Dreadcast: could not load texture from \"%s\"", resolved.c_str());
    }
    textures_[path] = tex;
    return tex;
}

SoundHandle ResourceManager::getSound(const std::string &path) {
    auto it = soundHandles_.find(path);
    if (it != soundHandles_.end()) {
        return it->second;
    }
    const std::string resolved = resolveAssetPath(path);
    const SoundHandle h = audio_.loadSound(resolved);
    if (h >= 0) {
        soundHandles_[path] = h;
    }
    return h;
}

void ResourceManager::loadUiFont(const std::string &path, int baseSize) {
    if (uiFontLoaded_) {
        return;
    }
    const std::string resolved = resolveAssetPath(path);
    std::vector<int> codepoints;
    codepoints.reserve(256);
    for (int c = 32; c <= 126; ++c) {
        codepoints.push_back(c);
    }
    for (int c = 0x00C0; c <= 0x00FF; ++c) {
        codepoints.push_back(c);
    }
    const int extras[] = {
        0x2013, // en dash
        0x2014, // em dash
        0x2018, 0x2019, // ‘ ’
        0x201C, 0x201D, // “ ”
        0x2026, // ellipsis
    };
    for (int c : extras) {
        codepoints.push_back(c);
    }
    const Font loaded =
        LoadFontEx(resolved.c_str(), baseSize, codepoints.data(),
                   static_cast<int>(codepoints.size()));
    if (loaded.glyphCount > 0) {
        uiFont_ = loaded;
        uiFontOwned_ = true;
        GenTextureMipmaps(&uiFont_.texture);
        SetTextureFilter(uiFont_.texture, TEXTURE_FILTER_TRILINEAR);
    } else {
        TraceLog(LOG_WARNING,
                 "Dreadcast: could not load UI font from \"%s\" (also tried next to executable). "
                 "Using Raylib default bitmap font.",
                 resolved.c_str());
        uiFont_ = GetFontDefault();
        uiFontOwned_ = false;
    }
    uiFontLoaded_ = true;
}

void ResourceManager::unloadAll() {
    soundHandles_.clear();
    audio_.shutdown();
    for (auto &kv : textures_) {
        UnloadTexture(kv.second);
    }
    textures_.clear();
    if (uiFontOwned_) {
        UnloadFont(uiFont_);
    }
    uiFont_ = {};
    uiFontLoaded_ = false;
    uiFontOwned_ = false;
}

} // namespace dreadcast
