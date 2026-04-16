#pragma once

#include <array>
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

inline bool operator==(const WallData &a, const WallData &b) {
    return a.cx == b.cx && a.cy == b.cy && a.halfW == b.halfW && a.halfH == b.halfH;
}

/// Walk-through hazard volume (same layout as walls; serialized as `LAVA`).
struct LavaData {
    float cx{0.0F};
    float cy{0.0F};
    float halfW{32.0F};
    float halfH{32.0F};
};

inline bool operator==(const LavaData &a, const LavaData &b) {
    return a.cx == b.cx && a.cy == b.cy && a.halfW == b.halfW && a.halfH == b.halfH;
}

struct EnemySpawnData {
    float x{0.0F};
    float y{0.0F};
    std::string type{"imp"};
};

inline bool operator==(const EnemySpawnData &a, const EnemySpawnData &b) {
    return a.x == b.x && a.y == b.y && a.type == b.type;
}

/// Ground item pickup placement (`ITEM x y kind` in `.map` files).
struct ItemSpawnData {
    float x{0.0F};
    float y{0.0F};
    std::string kind{"iron_armor"};
};

inline bool operator==(const ItemSpawnData &a, const ItemSpawnData &b) {
    return a.x == b.x && a.y == b.y && a.kind == b.kind;
}

/// Closed polygon obstacle (`SOLID` line in `.map`).
struct SolidShapeData {
    std::vector<Vector2> verts{};
};

inline bool operator==(const SolidShapeData &a, const SolidShapeData &b) {
    if (a.verts.size() != b.verts.size()) {
        return false;
    }
    for (size_t i = 0; i < a.verts.size(); ++i) {
        if (a.verts[i].x != b.verts[i].x || a.verts[i].y != b.verts[i].y) {
            return false;
        }
    }
    return true;
}

struct AnvilData {
    float cx{0.0F};
    float cy{0.0F};
};

inline bool operator==(const AnvilData &a, const AnvilData &b) {
    return a.cx == b.cx && a.cy == b.cy;
}

/// Old Casket position and up to three loot item ids (`CASKET cx cy [k0] [k1] [k2]`).
struct CasketData {
    float cx{1050.0F};
    float cy{0.0F};
    std::array<std::string, 3> itemSlots{};
};

inline bool operator==(const CasketData &a, const CasketData &b) {
    return a.cx == b.cx && a.cy == b.cy && a.itemSlots == b.itemSlots;
}

struct MapData {
    Vector2 playerSpawn{-100.0F, 0.0F};
    std::vector<WallData> walls{};
    std::vector<LavaData> lavas{};
    std::vector<EnemySpawnData> enemies{};
    std::vector<ItemSpawnData> itemSpawns{};
    std::vector<SolidShapeData> solidShapes{};
    std::vector<AnvilData> anvils{};
    CasketData casket{};
    bool hasCasket{true};

    [[nodiscard]] bool operator==(const MapData &o) const {
        return playerSpawn.x == o.playerSpawn.x && playerSpawn.y == o.playerSpawn.y &&
               hasCasket == o.hasCasket && casket == o.casket && walls == o.walls &&
               lavas == o.lavas && enemies == o.enemies && itemSpawns == o.itemSpawns &&
               solidShapes == o.solidShapes && anvils == o.anvils;
    }

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
        for (const SolidShapeData &sd : solidShapes) {
            if (sd.verts.size() < 3) {
                continue;
            }
            out << "SOLID";
            for (const Vector2 &v : sd.verts) {
                out << ' ' << v.x << ' ' << v.y;
            }
            out << '\n';
        }
        for (const EnemySpawnData &e : enemies) {
            out << "ENEMY " << e.x << ' ' << e.y << ' ' << e.type << '\n';
        }
        for (const ItemSpawnData &it : itemSpawns) {
            out << "ITEM " << it.x << ' ' << it.y << ' ' << it.kind << '\n';
        }
        for (const AnvilData &an : anvils) {
            out << "ANVIL " << an.cx << ' ' << an.cy << '\n';
        }
        if (hasCasket) {
            out << "CASKET " << casket.cx << ' ' << casket.cy;
            for (const std::string &k : casket.itemSlots) {
                out << ' ';
                if (k.empty()) {
                    out << '-';
                } else {
                    out << k;
                }
            }
            out << '\n';
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
            } else if (tag == "SOLID") {
                SolidShapeData sd{};
                float vx = 0.0F;
                float vy = 0.0F;
                while (iss >> vx >> vy) {
                    sd.verts.push_back(Vector2{vx, vy});
                }
                if (sd.verts.size() >= 3) {
                    parsed.solidShapes.push_back(std::move(sd));
                }
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
            } else if (tag == "ANVIL") {
                AnvilData an{};
                iss >> an.cx >> an.cy;
                parsed.anvils.push_back(an);
            } else if (tag == "CASKET") {
                iss >> parsed.casket.cx >> parsed.casket.cy;
                parsed.hasCasket = true;
                std::string tok;
                for (size_t i = 0; i < 3 && iss >> tok; ++i) {
                    if (tok != "-" && !tok.empty()) {
                        parsed.casket.itemSlots[i] = tok;
                    }
                }
                // Legacy `CASKET x y` only: keep classic two-drop defaults.
                if (parsed.casket.itemSlots[0].empty() && parsed.casket.itemSlots[1].empty() &&
                    parsed.casket.itemSlots[2].empty()) {
                    parsed.casket.itemSlots[0] = "iron_armor";
                    parsed.casket.itemSlots[1] = "vial_pure_blood";
                }
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
    map.casket.cx = 1050.0F;
    map.casket.cy = 0.0F;
    map.casket.itemSlots[0] = "iron_armor";
    map.casket.itemSlots[1] = "vial_pure_blood";
    map.casket.itemSlots[2] = "";
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
