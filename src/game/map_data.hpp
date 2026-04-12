#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <raylib.h>

namespace dreadcast {

struct WallData {
    float cx{0.0F};
    float cy{0.0F};
    float halfW{32.0F};
    float halfH{32.0F};
};

/// Walk-through hazard volume (same layout as walls; serialized as `LAVA`).
struct LavaData {
    float cx{0.0F};
    float cy{0.0F};
    float halfW{32.0F};
    float halfH{32.0F};
};

struct EnemySpawnData {
    float x{0.0F};
    float y{0.0F};
    std::string type{"imp"};
};

/// Ground item pickup placement (`ITEM x y kind` in `.map` files). `kind` values: `iron_armor`,
/// `barbed_tunic`, `runic_shell`, `hollow_ring`, `vial_pure_blood`, `vial_cordial_manic`,
/// `vial_raw_spirit` (see `makeItemFromMapKind`).
struct ItemSpawnData {
    float x{0.0F};
    float y{0.0F};
    std::string kind{"iron_armor"};
};

struct MapData {
    Vector2 playerSpawn{-100.0F, 0.0F};
    std::vector<WallData> walls{};
    std::vector<LavaData> lavas{};
    std::vector<EnemySpawnData> enemies{};
    std::vector<ItemSpawnData> itemSpawns{};
    Vector2 casketPos{1050.0F, 0.0F};
    bool hasCasket{true};

    bool saveToFile(const std::string &path) const {
        std::filesystem::path p(path);
        if (p.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);
            if (ec) {
                return false;
            }
        }
        std::ofstream out(path, std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out << "PLAYER_SPAWN " << playerSpawn.x << ' ' << playerSpawn.y << '\n';
        for (const WallData &w : walls) {
            out << "WALL " << w.cx << ' ' << w.cy << ' ' << w.halfW << ' ' << w.halfH << '\n';
        }
        for (const LavaData &lv : lavas) {
            out << "LAVA " << lv.cx << ' ' << lv.cy << ' ' << lv.halfW << ' ' << lv.halfH << '\n';
        }
        for (const EnemySpawnData &e : enemies) {
            out << "ENEMY " << e.x << ' ' << e.y << ' ' << e.type << '\n';
        }
        for (const ItemSpawnData &it : itemSpawns) {
            out << "ITEM " << it.x << ' ' << it.y << ' ' << it.kind << '\n';
        }
        if (hasCasket) {
            out << "CASKET " << casketPos.x << ' ' << casketPos.y << '\n';
        }
        return out.good();
    }

    bool loadFromFile(const std::string &path) {
        std::ifstream in(path);
        if (!in.is_open()) {
            return false;
        }
        MapData parsed{};
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            std::istringstream iss(line);
            std::string tag;
            iss >> tag;
            if (tag == "PLAYER_SPAWN") {
                iss >> parsed.playerSpawn.x >> parsed.playerSpawn.y;
            } else if (tag == "WALL") {
                WallData w{};
                iss >> w.cx >> w.cy >> w.halfW >> w.halfH;
                parsed.walls.push_back(w);
            } else if (tag == "LAVA") {
                LavaData lv{};
                iss >> lv.cx >> lv.cy >> lv.halfW >> lv.halfH;
                parsed.lavas.push_back(lv);
            } else if (tag == "ENEMY") {
                EnemySpawnData e{};
                iss >> e.x >> e.y;
                if (!(iss >> e.type)) {
                    e.type = "imp";
                }
                parsed.enemies.push_back(e);
            } else if (tag == "ITEM") {
                ItemSpawnData it{};
                iss >> it.x >> it.y >> it.kind;
                if (it.kind.empty()) {
                    it.kind = "iron_armor";
                }
                parsed.itemSpawns.push_back(it);
            } else if (tag == "CASKET") {
                iss >> parsed.casketPos.x >> parsed.casketPos.y;
                parsed.hasCasket = true;
            }
        }
        if (parsed.walls.empty()) {
            return false;
        }
        *this = std::move(parsed);
        return true;
    }
};

inline MapData defaultMapData() {
    MapData map{};
    map.playerSpawn = {-100.0F, 0.0F};
    map.hasCasket = true;
    map.casketPos = {1050.0F, 0.0F};
    map.walls = {
        {-220.0F, 0.0F, 22.0F, 190.0F},   {-40.0F, -210.0F, 220.0F, 22.0F},
        {-40.0F, 210.0F, 220.0F, 22.0F},   {180.0F, 95.0F, 22.0F, 75.0F},
        {180.0F, -95.0F, 22.0F, 75.0F},    {520.0F, -320.0F, 420.0F, 22.0F},
        {520.0F, 320.0F, 420.0F, 22.0F},   {930.0F, 135.0F, 22.0F, 100.0F},
        {930.0F, -135.0F, 22.0F, 100.0F},  {1040.0F, -210.0F, 220.0F, 22.0F},
        {1040.0F, 210.0F, 220.0F, 22.0F},  {1200.0F, 0.0F, 22.0F, 180.0F},
        {880.0F, 120.0F, 22.0F, 70.0F},    {880.0F, -120.0F, 22.0F, 70.0F},
    };
    map.enemies = {
        {450.0F, -200.0F, "imp"},
        {400.0F, 150.0F, "imp"},
        {650.0F, 100.0F, "imp"},
    };
    return map;
}

} // namespace dreadcast
