#include "ReadingStatsHoursActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <climits>
#include <cstdio>

#include "MappedInputManager.h"
#include "StatsText.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "stats/ReadingStats.h"
#include "stats/StatsQueries.h"

namespace fui = freeink::ui;

ReadingStatsHoursActivity::ReadingStatsHoursActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiTabListActivity("ReadingStatsHours", renderer, mappedInput) {
  tabPillMaxPad = 8;
}

void ReadingStatsHoursActivity::onEnter() {
  READING_STATS.setPaused(true);
  UiTabListActivity::onEnter();
  {
    RenderLock lock;
    loadGrid();
  }
  requestUpdate();
}

const char* ReadingStatsHoursActivity::headerTitle() const { return tr(STR_STATS_WHEN_I_READ); }

void ReadingStatsHoursActivity::loadGrid() {
  bool known = false;
  const int32_t todayDay = stats::today(known);
  int32_t fromDay = INT32_MIN;  // without a trusted clock every tab shows all time
  if (known && tab == TAB_30_DAYS) fromDay = todayDay - 29;
  if (known && tab == TAB_12_MONTHS) fromDay = todayDay - 364;
  stats::loadHourGrid(fromDay, grid);
  maxCell = *std::max_element(grid, grid + stats::GRID_CELLS);
}

const char* ReadingStatsHoursActivity::tabLabel(const int index) const {
  switch (index) {
    case TAB_12_MONTHS:
      return tr(STR_STATS_LAST_12_MONTHS);
    case TAB_ALL:
      return tr(STR_STATS_ALL_TIME);
    default:
      return tr(STR_STATS_LAST_30_DAYS);
  }
}

void ReadingStatsHoursActivity::selectTab(const int index) {
  if (index < 0 || index >= TAB_COUNT) return;
  const int ring = ringPos();  // the focused weekday stays across ranges
  {
    RenderLock lock;
    tab = index;
    loadGrid();
  }
  activeNav().selected = maxCell > 0 ? ring : 0;
  requestUpdate();
}

void ReadingStatsHoursActivity::onTabAction(const int index) {
  app.clearTapFlash();
  selectTab(index);
}

void ReadingStatsHoursActivity::stepTab(const int direction) {
  selectTab((tab + (direction > 0 ? 1 : TAB_COUNT - 1)) % TAB_COUNT);
}

bool ReadingStatsHoursActivity::handleButtons() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (ringPos() == 0) stepTab(1);
    return true;
  }
  return false;
}

void ReadingStatsHoursActivity::drawFooter() {
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), ringPos() == 0 ? tr(STR_TOGGLE) : "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void ReadingStatsHoursActivity::dayInfo(const int weekday, char* out, const size_t size) const {
  uint32_t total = 0;
  uint32_t best = 0;
  int bestHour = 0;
  for (int hour = 0; hour < 24; ++hour) {
    uint32_t value = 0;
    for (int d = 0; d < WEEKDAYS; ++d) {
      if (weekday < 0 || d == weekday) value += grid[d * 24 + hour];
    }
    total += value;
    if (value > best) {
      best = value;
      bestHour = hour;
    }
  }
  char name[16];
  if (weekday < 0) {
    snprintf(name, sizeof(name), "%s", tr(STR_STATS_ALL_DAYS));
  } else {
    statstext::weekdayName(weekday, name, sizeof(name));
  }
  char totalText[32];
  statstext::duration(total, totalText, sizeof(totalText));
  if (total == 0) {
    snprintf(out, size, "%s: %s", name, totalText);
    return;
  }
  char hourText[16];
  statstext::clockTime(bestHour * 60, hourText, sizeof(hourText));
  snprintf(out, size, tr(STR_STATS_GRID_INFO), name, totalText, hourText);
}

void ReadingStatsHoursActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int rightInset = renderer.getScreenWidth() - (safe.x + safe.width);
  const int bottomInset = renderer.getScreenHeight() - (safe.y + safe.height);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), static_cast<int16_t>(rightInset),
                  static_cast<int16_t>(bottomInset + metrics.buttonHintsHeight), static_cast<int16_t>(safe.x)});
  buildTabBar(screen);
  if (maxCell == 0) {
    screen.centeredText(tr(STR_STATS_NO_READING));
    return;
  }
  drawGrid(screen, screen.body());
}

void ReadingStatsHoursActivity::drawGrid(UiScreen& screen, const fui::Rect area) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int side = metrics.contentSidePadding;
  const int x0 = area.x + side;
  const int width = area.width - 2 * side;
  const int bottom = area.y + area.height;
  const int smallHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int focus = ringPos() - 1;  // weekday, -1 on the tabs
  int y = area.y + 8;

  char text[80];
  dayInfo(focus, text, sizeof(text));
  const std::string info = renderer.truncatedText(UI_12_FONT_ID, text, width, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID,
                    x0 + (width - renderer.getTextWidth(UI_12_FONT_ID, info.c_str(), EpdFontFamily::BOLD)) / 2, y,
                    info.c_str(), true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 12;

  char name[16];
  int labelWidth = 0;
  for (int d = 0; d < WEEKDAYS; ++d) {
    statstext::weekdayName(d, name, sizeof(name));
    labelWidth = std::max(labelWidth, renderer.getTextWidth(SMALL_FONT_ID, name, EpdFontFamily::BOLD));
  }
  labelWidth += 8;
  const int cellWidth = (width - labelWidth) / 24;
  const int cellHeight = std::min((bottom - y - smallHeight - 6) / WEEKDAYS, std::max(cellWidth * 2, smallHeight + 4));
  if (cellWidth < 3 || cellHeight < 4) return;
  const int gridX = x0 + width - cellWidth * 24;

  for (int d = 0; d < WEEKDAYS; ++d) {
    const int rowY = y + d * cellHeight;
    statstext::weekdayName(d, name, sizeof(name));
    const auto style = d == focus ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
    renderer.drawText(SMALL_FONT_ID, x0, rowY + (cellHeight - smallHeight) / 2, name, true, style);
    if (d == focus) renderer.fillRect(gridX - 5, rowY + 2, 3, cellHeight - 4, true);
    for (int hour = 0; hour < 24; ++hour) {
      const int cx = gridX + hour * cellWidth;
      switch (stats::gridShade(grid[d * 24 + hour], maxCell)) {
        case 3:
          renderer.fillRect(cx + 1, rowY + 1, cellWidth - 2, cellHeight - 2, true);
          break;
        case 2:
          renderer.fillRectDither(cx + 1, rowY + 1, cellWidth - 2, cellHeight - 2, Color::DarkGray);
          break;
        case 1:
          renderer.fillRectDither(cx + 1, rowY + 1, cellWidth - 2, cellHeight - 2, Color::LightGray);
          break;
        default:
          renderer.drawPixel(cx + cellWidth / 2, rowY + cellHeight / 2, true);
          break;
      }
    }
    // Tapping a row focuses that weekday.
    screen.frame().hit(fui::Rect{static_cast<int16_t>(x0), static_cast<int16_t>(rowY), static_cast<int16_t>(width),
                                 static_cast<int16_t>(cellHeight)},
                       ACTION_ROW, static_cast<int16_t>(d), fui::InputTouch);
  }
  const int axisY = y + WEEKDAYS * cellHeight + 4;
  for (int hour = 0; hour < 24; hour += 6) {
    char label[8];
    snprintf(label, sizeof(label), "%d", hour);
    renderer.drawText(SMALL_FONT_ID, gridX + hour * cellWidth, axisY, label, true);
  }
}
