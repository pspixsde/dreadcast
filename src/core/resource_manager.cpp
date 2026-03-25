#include "core/resource_manager.hpp"

#include <raylib.h>

#include <string>

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

ResourceManager::~ResourceManager() { unloadAll(); }

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

Sound ResourceManager::getSound(const std::string &path) {
    auto it = sounds_.find(path);
    if (it != sounds_.end()) {
        return it->second;
    }
    const Sound snd = LoadSound(path.c_str());
    sounds_[path] = snd;
    return snd;
}

void ResourceManager::loadUiFont(const std::string &path, int baseSize) {
    if (uiFontLoaded_) {
        return;
    }
    const std::string resolved = resolveAssetPath(path);
    const Font loaded = LoadFontEx(resolved.c_str(), baseSize, nullptr, 0);
    if (loaded.glyphCount > 0) {
        uiFont_ = loaded;
        uiFontOwned_ = true;
        SetTextureFilter(uiFont_.texture, TEXTURE_FILTER_BILINEAR);
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
    for (auto &kv : textures_) {
        UnloadTexture(kv.second);
    }
    textures_.clear();
    for (auto &kv : sounds_) {
        UnloadSound(kv.second);
    }
    sounds_.clear();
    if (uiFontOwned_) {
        UnloadFont(uiFont_);
    }
    uiFont_ = {};
    uiFontLoaded_ = false;
    uiFontOwned_ = false;
}

} // namespace dreadcast
