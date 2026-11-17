#include "ReadingStatsYearActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>
#include <string>
#include <vector>

#include "MappedInputManager.h"
#include "ReadingStatsBookActivity.h"
#include "StatsText.h"
#include "components/UITheme.h"
#include "stats/ReadingStats.h"
#include "stats/StatsQueries.h"

namespace fui = freeink::ui;

ReadingStatsYearActivity::ReadingStatsYearActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiTabListActivity("ReadingStatsYear", renderer, mappedInput) {
  tabPillMaxPad = 8;
}

const char* ReadingStatsYearActivity::headerTitle() const { return tr(STR_STATS_YEAR_IN_REVIEW); }

void ReadingStatsYearActivity::onEnter() {
  READING_STATS.setPaused(true);
  yearCount = stats::loadReadingYears(years, MAX_YEARS);
  for (int i = 0; i < yearCount; ++i) snprintf(yearLabels[i], sizeof(yearLabels[i]), "%d", years[i]);
  UiTabListActivity::onEnter();
  {
    RenderLock lock;
    loadYear();
  }
  requestUpdate();
}

const char* ReadingStatsYearActivity::tabLabel(const int index) const {
  return index >= 0 && index < yearCount ? yearLabels[index] : "-";
}

int ReadingStatsYearActivity::addRow(const char* label) {
  const int row = rowCount++;
  rows[row].label = label;
  rows[row].value = values[row];
  rows[row].subtitle = nullptr;
  rows[row].actionValue = static_cast<int16_t>(row);
  rowBook[row] = stats::NO_BOOK;
  snprintf(values[row], sizeof(values[row]), "-");
  return row;
}

void ReadingStatsYearActivity::loadYear() {
  rowCount = 0;
  if (tab >= yearCount) return;
  stats::loadYearReview(years[tab], review);
  if (review.sessions == 0) return;
  const stats::YearReview& y = review;

  int row = addRow(tr(STR_STATS_TOTAL));
  statstext::duration(y.activeSec, values[row], sizeof(values[row]));
  row = addRow(tr(STR_STATS_SESSIONS));
  snprintf(values[row], sizeof(values[row]), "%u", static_cast<unsigned>(y.sessions));
  row = addRow(tr(STR_STATS_AVERAGE_SESSION));
  statstext::duration(y.activeSec / y.sessions, values[row], sizeof(values[row]));
  row = addRow(tr(STR_STATS_BOOKS_FINISHED));
  snprintf(values[row], sizeof(values[row]), "%d", stats::countFinishedInYear(y.year));

  std::vector<uint16_t> indices(y.topBooks, y.topBooks + y.topCount);
  std::vector<std::string> titles;
  READING_STATS.bookTitles(indices, titles);
  for (int i = 0; i < y.topCount; ++i) {
    const char* title = i < static_cast<int>(titles.size()) && !titles[i].empty() ? titles[i].c_str()
                                                                                  : tr(STR_STATS_UNKNOWN_BOOK);
    snprintf(topLabels[i], sizeof(topLabels[i]), "%d. %s", i + 1, title);
    row = addRow(topLabels[i]);
    statstext::duration(y.topSec[i], values[row], sizeof(values[row]));
    rowBook[row] = y.topBooks[i];
  }

  const auto setBusiest = [this](const int at, const uint32_t seconds) {
    statstext::duration(seconds, subtitles[at], sizeof(subtitles[at]));
    rows[at].subtitle = subtitles[at];
  };
  row = addRow(tr(STR_STATS_BUSIEST_MONTH));
  const int month = stats::busiestIndex(y.months, 12);
  if (month >= 0) {
    statstext::monthName(static_cast<unsigned>(month) + 1, values[row], sizeof(values[row]));
    setBusiest(row, y.months[month]);
  }
  uint32_t weekdays[7];
  uint32_t hours[24];
  stats::gridTotals(y.grid, weekdays, hours);
  row = addRow(tr(STR_STATS_BUSIEST_WEEKDAY));
  const int weekday = stats::busiestIndex(weekdays, 7);
  if (weekday >= 0) {
    statstext::weekdayName(weekday, values[row], sizeof(values[row]));
    setBusiest(row, weekdays[weekday]);
  }
  row = addRow(tr(STR_STATS_FAVOURITE_HOUR));
  const int hour = stats::busiestIndex(hours, 24);
  if (hour >= 0) {
    statstext::clockTime(hour * 60, values[row], sizeof(values[row]));
    setBusiest(row, hours[hour]);
  }

  row = addRow(tr(STR_STATS_LONGEST_STREAK));
  if (y.longestStreak == 1) {
    snprintf(values[row], sizeof(values[row]), "%s", tr(STR_STATS_ONE_DAY));
  } else {
    snprintf(values[row], sizeof(values[row]), tr(STR_STATS_DAYS_COUNT), y.longestStreak);
  }

  row = addRow(tr(STR_STATS_LOOKUPS));
  snprintf(values[row], sizeof(values[row]), "%u", static_cast<unsigned>(y.lookups));
  char perHour[16];
  statstext::tenths(stats::lookupsPerHourTenths(y.lookups, y.activeSec), perHour, sizeof(perHour));
  snprintf(subtitles[row], sizeof(subtitles[row]), "%s: %s", tr(STR_STATS_LOOKUPS_PER_HOUR), perHour);
  rows[row].subtitle = subtitles[row];

  row = addRow(tr(STR_STATS_LATE_NIGHTS));
  statstext::sessionCount(y.lateSessions, values[row], sizeof(values[row]));
  if (y.lateSessions > 0) {
    statstext::lateDetail(y.lateSec, subtitles[row], sizeof(subtitles[row]));
    rows[row].subtitle = subtitles[row];
  }

  row = addRow(tr(STR_STATS_STYLES_MIX));
  int len = 0;
  subtitles[row][0] = '\0';
  for (int s = 1; s < stats::STYLE_COUNT; ++s) {
    if (y.styles[s] == 0 || len < 0 || static_cast<size_t>(len) >= sizeof(subtitles[row])) continue;
    len += snprintf(subtitles[row] + len, sizeof(subtitles[row]) - len, "%s%s %u", len > 0 ? " · " : "",
                    statstext::styleName(static_cast<stats::ReadingStyle>(s)), static_cast<unsigned>(y.styles[s]));
  }
  if (len > 0) rows[row].subtitle = subtitles[row];
}

void ReadingStatsYearActivity::selectTab(const int index) {
  if (index < 0 || index >= tabCount()) return;
  {
    RenderLock lock;
    tab = index;
    loadYear();
  }
  activeNav().selected = 0;
  activeNav().top = 0;
  requestUpdate();
}

void ReadingStatsYearActivity::onTabAction(const int index) {
  app.clearTapFlash();
  selectTab(index);
}

void ReadingStatsYearActivity::stepTab(const int direction) {
  selectTab((tab + (direction > 0 ? 1 : tabCount() - 1)) % tabCount());
}

void ReadingStatsYearActivity::activateIndex(const int index) {
  if (index < 0 || index >= rowCount || rowBook[index] == stats::NO_BOOK) return;
  app.clearTapFlash();
  ReadingStatsBookActivity::push(*this, renderer, mappedInput, rowBook[index]);
}

bool ReadingStatsYearActivity::handleButtons() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    const int ring = ringPos();
    if (ring == 0) {
      stepTab(1);
    } else if (ring - 1 < listCount()) {
      activateIndex(ring - 1);
    }
    return true;
  }
  return false;
}

void ReadingStatsYearActivity::drawFooter() {
  const int ring = ringPos();
  const char* confirm = "";
  if (ring == 0) {
    confirm = tr(STR_TOGGLE);
  } else if (ring - 1 < rowCount && rowBook[ring - 1] != stats::NO_BOOK) {
    confirm = tr(STR_OPEN);
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirm, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void ReadingStatsYearActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int rightInset = renderer.getScreenWidth() - (safe.x + safe.width);
  const int bottomInset = renderer.getScreenHeight() - (safe.y + safe.height);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), static_cast<int16_t>(rightInset),
                  static_cast<int16_t>(bottomInset + metrics.buttonHintsHeight), static_cast<int16_t>(safe.x)});
  buildTabBar(screen);
  if (rowCount == 0) {
    screen.centeredText(yearCount > 0 ? tr(STR_STATS_NO_READING) : tr(STR_STATS_NO_BOOKS));
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
