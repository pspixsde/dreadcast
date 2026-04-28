#include "ui/skill_tree_ui.hpp"

#include <cstdio>
#include <cmath>

#include "core/input.hpp"
#include "core/resource_manager.hpp"
#include "ui/theme.hpp"

#include <raylib.h>

namespace dreadcast::ui {

namespace {

constexpr float kPanelW = 1020.0F;
constexpr float kPanelH = 780.0F;
constexpr float kNodeRMajor = 50.0F;
constexpr float kNodeRMinor = 38.0F;
constexpr float kCoreDx = 95.0F;
constexpr float kDiag = 118.0F;
constexpr float kMajorIconOutlineThickness = 2.0F;

[[nodiscard]] Shader majorIconCircleMaskShader() {
    static Shader shader = LoadShaderFromMemory(
        nullptr, R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
void main() {
    vec2 d = fragTexCoord - vec2(0.5, 0.5);
    float r2 = dot(d, d);
    if (r2 > 0.25) {
        discard;
    }
    finalColor = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
}
)");
    return shader;
}

struct NodeDef {
    float ox;
    float oy;
    SkillNodeType type;
    const char *name;
    const char *description;
    const char *iconPath;
    int cost;
};

constexpr std::array<NodeDef, kSkillTreeNodeCount> kNodes{{
    {-kCoreDx, 0.0F, SkillNodeType::Major, "Bottomless Chamber",
     "Simple ranged attacks are free and infinite. Reload rhythm still applies.",
     "assets/textures/skills/bottomless_chamber.png", 0},
    {kCoreDx, 0.0F, SkillNodeType::Major, "Last Hand",
     "Enables close-defense fallback so you can fight while out of bullets and mana.",
     "assets/textures/skills/last_hand.png", 0},
    {kCoreDx + kDiag, -kDiag, SkillNodeType::Minor, "Heavy Round",
     "+2 to base ranged attack damage.", "assets/textures/skills/flying_pellet.png", 1},
    {kCoreDx + kDiag, kDiag, SkillNodeType::Minor, "Hot Lead",
     "+40 base ranged attack projectile speed.", "assets/textures/skills/flying_pellet.png", 1},
    {-kCoreDx - kDiag, -kDiag, SkillNodeType::Minor, "Long Reach",
     "+8 base melee attack reach.", "assets/textures/skills/melee_sword.png", 1},
    {-kCoreDx - kDiag, kDiag, SkillNodeType::Minor, "Heavy Strike",
     "+5 to base melee attack damage.", "assets/textures/skills/melee_sword.png", 1},
}};

/// Ranged minors (2,3) stem from melee core (1); melee minors (4,5) stem from ranged core (0).
constexpr int kConn[][2] = {{0, 1}, {1, 2}, {1, 3}, {0, 4}, {0, 5}};

[[nodiscard]] float nodeRadius(int idx) {
    return kNodes[static_cast<size_t>(idx)].type == SkillNodeType::Major ? kNodeRMajor
                                                                          : kNodeRMinor;
}

[[nodiscard]] bool neighborOfLearned(const std::array<bool, kSkillTreeNodeCount> &learned, int idx) {
    for (const auto &c : kConn) {
        const int a = c[0];
        const int b = c[1];
        if (a == idx && learned[static_cast<size_t>(b)]) {
            return true;
        }
        if (b == idx && learned[static_cast<size_t>(a)]) {
            return true;
        }
    }
    return false;
}

} // namespace

void SkillTreeUI::resetProgress() {
    learned_.fill(false);
    learned_[0] = learned_[1] = true;
}

void SkillTreeUI::update(InputManager &input, int &skillPoints, float frameDt, bool &consumeEscOut,
                         bool &flashNoPoints) {
    consumeEscOut = false;
    flashNoPoints = false;
    if (!open_) {
        holdEProgress_ = 0.0F;
        hoverNode_ = -1;
        return;
    }

    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    const float cx = static_cast<float>(w) * 0.5F;
    const float cy = static_cast<float>(h) * 0.5F;
    const Vector2 m = input.mousePosition();

    hoverNode_ = -1;
    for (int i = 0; i < kSkillTreeNodeCount; ++i) {
        const float nx = cx + kNodes[static_cast<size_t>(i)].ox;
        const float ny = cy + kNodes[static_cast<size_t>(i)].oy;
        const float nr = nodeRadius(i);
        const float dx = m.x - nx;
        const float dy = m.y - ny;
        if (dx * dx + dy * dy <= nr * nr) {
            hoverNode_ = i;
            break;
        }
    }

    const bool canSpend = hoverNode_ >= 0 && !learned_[static_cast<size_t>(hoverNode_)] &&
                          kNodes[static_cast<size_t>(hoverNode_)].type != SkillNodeType::Major &&
                          neighborOfLearned(learned_, hoverNode_);

    if (input.isKeyPressed(KEY_ESCAPE)) {
        open_ = false;
        consumeEscOut = true;
        holdEProgress_ = 0.0F;
        return;
    }

    if (canSpend && input.isKeyHeld(KEY_E)) {
        if (skillPoints <= 0) {
            flashNoPoints = true;
            holdEProgress_ = 0.0F;
        } else {
            holdEProgress_ += frameDt;
            if (holdEProgress_ >= 1.0F) {
                learned_[static_cast<size_t>(hoverNode_)] = true;
                --skillPoints;
                holdEProgress_ = 0.0F;
            }
        }
    } else {
        holdEProgress_ = 0.0F;
    }
}

void SkillTreeUI::draw(const Font &font, ResourceManager &resources, int skillPoints,
                       float noSkillPointFlashTimer) {
    if (!open_) {
        return;
    }

    const int w = GetScreenWidth();
    const int h = GetScreenHeight();
    const float cx = static_cast<float>(w) * 0.5F;
    const float cy = static_cast<float>(h) * 0.5F;
    const Rectangle panel{(static_cast<float>(w) - kPanelW) * 0.5F,
                          (static_cast<float>(h) - kPanelH) * 0.5F, kPanelW, kPanelH};

    DrawRectangleRec(panel, Color{20, 16, 14, 240});
    DrawRectangleLinesEx(panel, 3.0F, ui::theme::BTN_BORDER);

    const char *title = "Skill Tree — Undead Hunter";
    const float titleSz = 26.0F;
    DrawTextEx(font, title, {panel.x + 24.0F, panel.y + 18.0F}, titleSz, 1.0F, RAYWHITE);

    char spBuf[48];
    std::snprintf(spBuf, sizeof(spBuf), "Skill points: %d", skillPoints);
    const float spSz = 22.0F;
    const Vector2 spd = MeasureTextEx(font, spBuf, spSz, 1.0F);
    DrawTextEx(font, spBuf, {panel.x + kPanelW - spd.x - 24.0F, panel.y + 20.0F}, spSz, 1.0F,
               Color{210, 175, 95, 255});

    if (noSkillPointFlashTimer > 0.001F) {
        const char *msg = "No skill points";
        const float ms = 20.0F;
        const Vector2 md = MeasureTextEx(font, msg, ms, 1.0F);
        DrawTextEx(font, msg,
                   {panel.x + (kPanelW - md.x) * 0.5F, panel.y + kPanelH - 48.0F}, ms, 1.0F,
                   Color{220, 80, 80, 255});
    }

    auto nodeCenter = [&](int i) {
        return Vector2{cx + kNodes[static_cast<size_t>(i)].ox, cy + kNodes[static_cast<size_t>(i)].oy};
    };

    for (const auto &c : kConn) {
        const Vector2 a = nodeCenter(c[0]);
        const Vector2 b = nodeCenter(c[1]);
        DrawLineEx(a, b, 3.0F, Fade(WHITE, 0.35F));
    }

    for (int i = 0; i < kSkillTreeNodeCount; ++i) {
        const Vector2 nc = nodeCenter(i);
        const float nr = nodeRadius(i);
        const bool learned = learned_[static_cast<size_t>(i)];
        const bool major = kNodes[static_cast<size_t>(i)].type == SkillNodeType::Major;
        const bool grey = !learned && !major;
        Color fill = major ? Color{90, 70, 45, 255}
                           : (learned ? Color{70, 85, 60, 255} : Color{45, 45, 48, 255});
        // Keep node interiors fully opaque so connector lines always stay visually "under" nodes.
        DrawCircleV(nc, nr, fill);
        const Color majorOutline = learned ? Color{85, 195, 95, 255} : Color{220, 190, 120, 255};
        DrawCircleLinesV(nc, nr, major ? majorOutline : Fade(WHITE, 0.35F));

        const char *ip = kNodes[static_cast<size_t>(i)].iconPath;
        const Texture2D tex = resources.getTexture(ip);
        if (tex.id != 0) {
            // Major icons: fill full circle diameter footprint and mask to a clean circle.
            // Minor icons keep a slight inset and square crop.
            const float side = major ? (nr * 1.82F) : (nr * 0.96F);
            const Rectangle dst{nc.x - side * 0.5F, nc.y - side * 0.5F, side, side};
            const float minSide = static_cast<float>(std::min(tex.width, tex.height));
            const Rectangle src{(static_cast<float>(tex.width) - minSide) * 0.5F,
                                (static_cast<float>(tex.height) - minSide) * 0.5F, minSide,
                                minSide};
            const Color tint = grey ? Color{150, 150, 150, 255} : WHITE;
            if (major) {
                BeginShaderMode(majorIconCircleMaskShader());
                DrawTexturePro(tex, src, dst, {0.0F, 0.0F}, 0.0F, tint);
                EndShaderMode();
                DrawRing(nc, nr - kMajorIconOutlineThickness, nr, 0.0F, 360.0F, 48,
                         majorOutline);
            } else {
                DrawTexturePro(tex, src, dst, {0.0F, 0.0F}, 0.0F, tint);
            }
        }
    }

    if (hoverNode_ >= 0 && holdEProgress_ > 0.001F && learned_[static_cast<size_t>(hoverNode_)] == false &&
        kNodes[static_cast<size_t>(hoverNode_)].type != SkillNodeType::Major) {
        const Vector2 nc = nodeCenter(hoverNode_);
        const float nr = nodeRadius(hoverNode_);
        const float t = std::clamp(holdEProgress_, 0.0F, 1.0F);
        const float a0 = -90.0F;
        const float a1 = -90.0F + 360.0F * t;
        DrawRing(nc, nr + 4.0F, nr + 7.0F, a0, a1, 32, Color{210, 175, 95, 220});
    }

    if (hoverNode_ >= 0) {
        const size_t idx = static_cast<size_t>(hoverNode_);
        const NodeDef &node = kNodes[idx];
        const bool learned = learned_[idx];
        const bool eligible =
            !learned && node.type != SkillNodeType::Major && neighborOfLearned(learned_, hoverNode_);

        const float titleFs = 22.0F;
        const float bodyFs = 18.0F;
        const float statusFs = 16.0F;
        const float pad = 14.0F;
        const Vector2 td = MeasureTextEx(font, node.name, titleFs, 1.0F);
        const Vector2 dd = MeasureTextEx(font, node.description, bodyFs, 1.0F);

        char typeBuf[24];
        std::snprintf(typeBuf, sizeof(typeBuf), "%s",
                      node.type == SkillNodeType::Major ? "Major" : "Minor");
        const Vector2 typeDim = MeasureTextEx(font, typeBuf, statusFs, 1.0F);

        char statusBuf[48];
        if (learned) {
            std::snprintf(statusBuf, sizeof(statusBuf), "Skilled");
        } else if (eligible) {
            std::snprintf(statusBuf, sizeof(statusBuf), "%d SP - Hold [E]", node.cost);
        } else {
            std::snprintf(statusBuf, sizeof(statusBuf), "Locked");
        }
        const char *status = statusBuf;
        const Color statusCol =
            learned ? Color{120, 180, 110, 255}
                    : (eligible ? Color{210, 175, 95, 255} : Color{160, 160, 160, 255});
        const Vector2 sd = MeasureTextEx(font, status, statusFs, 1.0F);

        Rectangle tip{0.0F, 0.0F,
                      std::max(380.0F, std::max(td.x + pad * 2.0F, dd.x + pad * 2.0F)),
                      pad * 4.0F + td.y + dd.y + typeDim.y};
        const Vector2 nc = nodeCenter(hoverNode_);
        const float nrTip = nodeRadius(hoverNode_);
        tip.x = nc.x + nrTip + 20.0F;
        tip.y = nc.y - tip.height - 16.0F;
        if (tip.x + tip.width > panel.x + panel.width - 12.0F) {
            tip.x = nc.x - tip.width - nrTip - 34.0F;
        }
        tip.x = std::clamp(tip.x, panel.x + 8.0F, panel.x + panel.width - tip.width - 8.0F);
        tip.y = std::clamp(tip.y, panel.y + 56.0F, panel.y + panel.height - tip.height - 8.0F);

        DrawRectangleRec(tip, Color{27, 22, 20, 250});
        DrawRectangleLinesEx(tip, 2.0F, ui::theme::BTN_BORDER);
        DrawTextEx(font, node.name, {tip.x + pad, tip.y + pad}, titleFs, 1.0F, RAYWHITE);
        DrawTextEx(font, typeBuf, {tip.x + pad, tip.y + pad + td.y + 4.0F}, statusFs, 1.0F,
                   Fade(Color{180, 160, 120, 255}, 0.95F));
        DrawTextEx(font, status, {tip.x + tip.width - sd.x - pad, tip.y + pad + 2.0F}, statusFs, 1.0F,
                   statusCol);
        DrawTextEx(font, node.description,
                   {tip.x + pad, tip.y + pad * 2.0F + td.y + typeDim.y + 8.0F}, bodyFs, 1.0F,
                   Fade(RAYWHITE, 0.9F));
    }
}

[[nodiscard]] const std::array<SkillArchiveEntry, kSkillTreeNodeCount> &skill_tree_archive_entries() {
    static const std::array<SkillArchiveEntry, kSkillTreeNodeCount> kArchive{{
        {kNodes[0].name, kNodes[0].description, kNodes[0].type, kNodes[0].iconPath},
        {kNodes[1].name, kNodes[1].description, kNodes[1].type, kNodes[1].iconPath},
        {kNodes[2].name, kNodes[2].description, kNodes[2].type, kNodes[2].iconPath},
        {kNodes[3].name, kNodes[3].description, kNodes[3].type, kNodes[3].iconPath},
        {kNodes[4].name, kNodes[4].description, kNodes[4].type, kNodes[4].iconPath},
        {kNodes[5].name, kNodes[5].description, kNodes[5].type, kNodes[5].iconPath},
    }};
    return kArchive;
}

} // namespace dreadcast::ui
