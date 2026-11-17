#pragma once

#include <GfxRenderer.h>

#include "components/UITheme.h"
#include "fontIds.h"

// Outlined button of the dictionary screens (definition sheet, "Not Found");
// pressed = filled black with white text.
inline void drawLookupButton(const GfxRenderer& renderer, const Rect& box, const char* label, const bool pressed) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  renderer.fillRoundedRect(box.x, box.y, box.width, box.height, metrics.controlRadius,
                           pressed ? Color::Black : Color::White);
  if (!pressed) renderer.drawRoundedRect(box.x, box.y, box.width, box.height, 1, metrics.controlRadius, true);
  const int textX = box.x + (box.width - renderer.getTextWidth(UI_10_FONT_ID, label)) / 2;
  const int textY = box.y + (box.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2;
  renderer.drawText(UI_10_FONT_ID, textX, textY, label, !pressed);
}
