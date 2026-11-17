#include "ReadingStatsSpeedActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "StatsText.h"
#include "components/UITheme.h"
#include "stats/ReadingStats.h"
#include "stats/StatsQueries.h"

namespace fui = freeink::ui;

ReadingStatsSpeedActivity::ReadingStatsSpeedActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiTabListActivity("ReadingStatsSpeed", renderer, mappedInput) {
  tabPillMaxPad = 8;
}

const char* ReadingStatsSpeedActivity::headerTitle() const { return tr(STR_STATS_SPEED_TRENDS); }

void ReadingStatsSpeedActivity::onEnter() {
  READING_STATS.setPaused(true);
  UiTabListActivity::onEnter();
  {
    RenderLock lock;
    stats::loadSpeedTrends(trends);
    formatRows();
  }
  requestUpdate();
}

const char* ReadingStatsSpeedActivity::tabLabel(const int index) const {
  switch (index) {
    case TAB_DAY_PARTS:
      return tr(STR_STATS_TIME_OF_DAY);
    case TAB_FONTS:
      return tr(STR_STATS_FONT_SIZE);
    default:
      return tr(STR_STATS_MONTHS);
  }
}

void ReadingStatsSpeedActivity::setRow(const int row, const char* label, const stats::SpeedCell& cell) {
  snprintf(labels[row], sizeof(labels[row]), "%s", label);
  const uint32_t speed = stats::cellPagesPerHour(cell);
  if (speed > 0) {
    snprintf(values[row], sizeof(values[row]), tr(STR_STATS_PAGES_PER_HOUR), static_cast<unsigned>(speed));
  } else {
    snprintf(values[row], sizeof(values[row]), "-");
  }
  subtitles[row][0] = '\0';
  if (cell.learnedMs > 0) {
    char measured[24];
    statstext::duration(static_cast<uint32_t>(std::min<uint64_t>(cell.learnedMs / 1000, UINT32_MAX)), measured,
                       sizeof(measured));
    snprintf(subtitles[row], sizeof(subtitles[row]), tr(STR_STATS_MEASURED), measured);
  }
  rows[row].label = labels[row];
  rows[row].value = values[row];
  rows[row].subtitle = subtitles[row][0] ? subtitles[row] : nullptr;
  rows[row].actionValue = static_cast<int16_t>(row);
}

void ReadingStatsSpeedActivity::formatRows() {
  char label[24];
  rowCount = 0;
  switch (tab) {
    case TAB_MONTHS:
      // Newest month first.
      for (int i = stats::SPEED_MONTHS - 1; i >= 0; --i) {
        const int32_t month = trends.firstMonth + i;
        char name[16];
        statstext::monthName(static_cast<unsigned>(month % 12) + 1, name, sizeof(name));
        snprintf(label, sizeof(label), "%s %d", name, static_cast<int>(month / 12));
        setRow(rowCount++, label, trends.months[i]);
      }
      break;
    case TAB_DAY_PARTS: {
      const char* names[] = {tr(STR_STATS_MORNING), tr(STR_STATS_AFTERNOON), tr(STR_STATS_EVENING),
                             tr(STR_STATS_NIGHT)};
      for (int i = 0; i < stats::DAY_PART_COUNT; ++i) setRow(rowCount++, names[i], trends.dayParts[i]);
      break;
    }
    default:
      for (int i = 0; i < trends.fontCount; ++i) {
        snprintf(label, sizeof(label), tr(STR_STATS_FONT_PT), static_cast<unsigned>(trends.fontSizes[i]));
        setRow(rowCount++, label, trends.fonts[i]);
      }
      break;
  }
}

void ReadingStatsSpeedActivity::selectTab(const int index) {
  if (index < 0 || index >= TAB_COUNT) return;
  {
    RenderLock lock;
    tab = index;
    formatRows();
  }
  activeNav().selected = 0;
  activeNav().top = 0;
  requestUpdate();
}

void ReadingStatsSpeedActivity::onTabAction(const int index) {
  app.clearTapFlash();
  selectTab(index);
}

void ReadingStatsSpeedActivity::stepTab(const int direction) {
  selectTab((tab + (direction > 0 ? 1 : TAB_COUNT - 1)) % TAB_COUNT);
}

bool ReadingStatsSpeedActivity::handleButtons() {
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

void ReadingStatsSpeedActivity::drawFooter() {
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), ringPos() == 0 ? tr(STR_TOGGLE) : "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void ReadingStatsSpeedActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int rightInset = renderer.getScreenWidth() - (safe.x + safe.width);
  const int bottomInset = renderer.getScreenHeight() - (safe.y + safe.height);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), static_cast<int16_t>(rightInset),
                  static_cast<int16_t>(bottomInset + metrics.buttonHintsHeight), static_cast<int16_t>(safe.x)});
  buildTabBar(screen);
  if (rowCount == 0) {
    screen.centeredText(tr(STR_STATS_NO_READING));
    return;
  }
  fui::ListProps props;
  props.items = rows;
  props.count = static_cast<uint16_t>(rowCount);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncTabListViewport(screen, props);
  screen.list(props);
}
