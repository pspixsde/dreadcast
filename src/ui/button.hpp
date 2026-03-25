#pragma once

#include <raylib.h>

namespace dreadcast::ui {

/// Simple screen-space button (hover + click).
struct Button {
    Rectangle rect{};
    const char *label = "";

    [[nodiscard]] bool isHovered(Vector2 mouse) const {
        return CheckCollisionPointRec(mouse, rect);
    }

    [[nodiscard]] bool wasClicked(Vector2 mouse, bool mousePressed) const {
        return mousePressed && isHovered(mouse);
    }

    void draw(const Font &font, float fontSize, Vector2 mouse, Color fill, Color fillHover,
              Color textColor, Color border) const;
};

} // namespace dreadcast::ui
