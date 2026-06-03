#pragma once

#include <array>
#include <cmath>
#include <cstdint>
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
    /// Rotation about the center in radians (0 = axis-aligned).
    float angle{0.0F};
};

inline bool operator==(const WallData &a, const WallData &b) {
    return a.cx == b.cx && a.cy == b.cy && a.halfW == b.halfW && a.halfH == b.halfH &&
           a.angle == b.angle;
}

/// World-space corners of an oriented box (TL, TR, BR, BL in local order, rotated by `angle`).
inline std::array<Vector2, 4> orientedBoxCorners(float cx, float cy, float halfW, float halfH,
                                                  float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const auto rot = [&](float lx, float ly) -> Vector2 {
        return {cx + lx * c - ly * s, cy + lx * s + ly * c};
    };
    return {rot(-halfW, -halfH), rot(halfW, -halfH), rot(halfW, halfH), rot(-halfW, halfH)};
}

inline std::array<Vector2, 4> wallCorners(const WallData &w) {
    return orientedBoxCorners(w.cx, w.cy, w.halfW, w.halfH, w.angle);
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

/// Dreg spawner placement (`NODE cx cy` in `.map` files). Tuning is fixed in `NodeSpawner`.
struct NodeData {
    float cx{0.0F};
    float cy{0.0F};
};

inline bool operator==(const NodeData &a, const NodeData &b) {
    return a.cx == b.cx && a.cy == b.cy;
}

/// Loot casket tiers. Contents are no longer authored in the editor; the tier alone decides the
/// per-session roll (item-count odds, per-slot rarity odds, minimum-rarity guarantee). See
/// `rollCasketLoot` in `game/game_data.hpp`.
enum class CasketTier : std::uint8_t { Old = 0, Sealed, Wrought };

/// Lowercase token used in `.map` files (`CASKET cx cy <token>`).
[[nodiscard]] inline const char *casketTierToken(CasketTier t) {
    switch (t) {
    case CasketTier::Sealed:
        return "sealed";
    case CasketTier::Wrought:
        return "wrought";
    case CasketTier::Old:
    default:
        return "old";
    }
}

/// Human-readable name shown on the interaction prompt and editor labels.
[[nodiscard]] inline const char *casketTierDisplayName(CasketTier t) {
    switch (t) {
    case CasketTier::Sealed:
        return "Sealed Casket";
    case CasketTier::Wrought:
        return "Wrought Casket";
    case CasketTier::Old:
    default:
        return "Old Casket";
    }
}

/// Tint used for the casket sprite/editor quad so tiers read apart at a glance.
[[nodiscard]] inline Color casketTierColor(CasketTier t) {
    switch (t) {
    case CasketTier::Sealed:
        return {72, 98, 124, 255};
    case CasketTier::Wrought:
        return {126, 96, 48, 255};
    case CasketTier::Old:
    default:
        return {90, 70, 55, 255};
    }
}

/// Parses a `.map` casket tier token. Returns false for anything else (e.g. legacy item ids).
[[nodiscard]] inline bool casketTierFromToken(const std::string &s, CasketTier &out) {
    if (s == "old") {
        out = CasketTier::Old;
        return true;
    }
    if (s == "sealed") {
        out = CasketTier::Sealed;
        return true;
    }
    if (s == "wrought") {
        out = CasketTier::Wrought;
        return true;
    }
    return false;
}

/// Casket placement + loot tier (`CASKET cx cy <tier>`). Loot is rolled per session at spawn.
struct CasketData {
    float cx{1050.0F};
    float cy{0.0F};
    CasketTier tier{CasketTier::Old};
};

inline bool operator==(const CasketData &a, const CasketData &b) {
    return a.cx == b.cx && a.cy == b.cy && a.tier == b.tier;
}

struct MapData {
    Vector2 playerSpawn{-100.0F, 0.0F};
    std::vector<WallData> walls{};
    std::vector<LavaData> lavas{};
    std::vector<EnemySpawnData> enemies{};
    std::vector<ItemSpawnData> itemSpawns{};
    std::vector<SolidShapeData> solidShapes{};
    std::vector<AnvilData> anvils{};
    std::vector<CasketData> caskets{};
    std::vector<NodeData> nodes{};

    [[nodiscard]] bool operator==(const MapData &o) const {
        return playerSpawn.x == o.playerSpawn.x && playerSpawn.y == o.playerSpawn.y &&
               caskets == o.caskets && walls == o.walls &&
               lavas == o.lavas && enemies == o.enemies && itemSpawns == o.itemSpawns &&
               solidShapes == o.solidShapes && anvils == o.anvils && nodes == o.nodes;
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
            out << "WALL " << w.cx << ' ' << w.cy << ' ' << w.halfW << ' ' << w.halfH;
            if (w.angle != 0.0F) {
                out << ' ' << w.angle;
            }
            out << '\n';
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
        for (const NodeData &nd : nodes) {
            out << "NODE " << nd.cx << ' ' << nd.cy << '\n';
        }
        for (const CasketData &ck : caskets) {
            out << "CASKET " << ck.cx << ' ' << ck.cy << ' ' << casketTierToken(ck.tier) << '\n';
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
                if (!(iss >> w.angle)) {
                    w.angle = 0.0F;
                }
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
            } else if (tag == "NODE") {
                NodeData nd{};
                iss >> nd.cx >> nd.cy;
                parsed.nodes.push_back(nd);
            } else if (tag == "CASKET") {
                CasketData ck{};
                iss >> ck.cx >> ck.cy;
                // Tier token may sit anywhere after the coords; legacy fixed-loot item ids (and the
                // `-` placeholder) are ignored, leaving the default `Old` tier.
                std::string tok;
                while (iss >> tok) {
                    CasketTier t{};
                    if (casketTierFromToken(tok, t)) {
                        ck.tier = t;
                        break;
                    }
                }
                parsed.caskets.push_back(ck);
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
    CasketData defaultCasket{};
    defaultCasket.cx = 1050.0F;
    defaultCasket.cy = 0.0F;
    defaultCasket.tier = CasketTier::Old;
    map.caskets.push_back(defaultCasket);
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
