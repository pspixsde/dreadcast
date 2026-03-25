#include "ui/button.hpp"

#include <cmath>

namespace dreadcast::ui {

void Button::draw(const Font &font, float fontSize, Vector2 mouse, Color fill, Color fillHover,
                  Color textColor, Color border) const {
    const bool hover = isHovered(mouse);
    const Color bg = hover ? fillHover : fill;
    DrawRectangleRec(rect, bg);
    DrawRectangleLinesEx(rect, 2.0F, border);

    const Vector2 textSize = MeasureTextEx(font, label, fontSize, 1.0F);
    const float tx = rect.x + (rect.width - textSize.x) * 0.5F;
    const float ty = rect.y + (rect.height - textSize.y) * 0.5F;
    DrawTextEx(font, label, {tx, ty}, fontSize, 1.0F, textColor);
}

} // namespace dreadcast::ui
