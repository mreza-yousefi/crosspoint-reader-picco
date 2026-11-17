#include "ReadingStatsRecordsActivity.h"

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

ReadingStatsRecordsActivity::ReadingStatsRecordsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("ReadingStatsRecords", renderer, mappedInput) {}

const char* ReadingStatsRecordsActivity::headerTitle() const { return tr(STR_STATS_RECORDS); }

void ReadingStatsRecordsActivity::onEnter() {
  READING_STATS.setPaused(true);
  stats::loadRecords(records);
  for (int i = 0; i < ROW_COUNT; ++i) {
    rows[i].actionValue = static_cast<int16_t>(i);
    rowBook[i] = stats::NO_BOOK;
  }
  rows[ROW_LONGEST_SESSION].label = tr(STR_STATS_LONGEST_SESSION);
  rows[ROW_LONGEST_SITTING].label = tr(STR_STATS_LONGEST_SITTING);
  rows[ROW_MOST_DAY].label = tr(STR_STATS_MOST_IN_DAY);
  rows[ROW_MOST_WEEK].label = tr(STR_STATS_MOST_IN_WEEK);
  rows[ROW_FASTEST].label = tr(STR_STATS_FASTEST_BOOK);
  rows[ROW_STREAK].label = tr(STR_STATS_LONGEST_STREAK);
  rows[ROW_MOST_LOOKUPS].label = tr(STR_STATS_MOST_LOOKUPS_SESSION);
  rows[ROW_LATEST].label = tr(STR_STATS_LATEST_NIGHT);
  rows[ROW_EARLIEST].label = tr(STR_STATS_EARLIEST_MORNING);
  rows[ROW_LOOKUPS_PER_HOUR].label = tr(STR_STATS_LOOKUPS_PER_HOUR);
  rows[ROW_LATE].label = tr(STR_STATS_LATE_NIGHTS);
  format();
  UiListActivity::onEnter();
}

void ReadingStatsRecordsActivity::setBookRecord(const int row, const stats::RecordRef& record, const char* value,
                                                const char* title) {
  rows[row].value = values[row];
  if (record.book == stats::NO_BOOK) {
    snprintf(values[row], sizeof(values[row]), "-");
    return;
  }
  rowBook[row] = record.book;
  snprintf(values[row], sizeof(values[row]), "%s", value);
  if (record.utc != 0) {
    char day[32];
    statstext::dayLong(stats::localDay(record.utc, record.tzOffsetMin), true, day, sizeof(day));
    snprintf(subtitles[row], sizeof(subtitles[row]), "%s · %s", title, day);
  } else {
    snprintf(subtitles[row], sizeof(subtitles[row]), "%s", title);
  }
  rows[row].subtitle = subtitles[row];
}

void ReadingStatsRecordsActivity::format() {
  const stats::Records& r = records;
  // One registry read for the titles of every record.
  const stats::RecordRef* refs[] = {&r.longestSession, &r.longestSitting, &r.fastestBook,
                                    &r.mostLookups,    &r.latestNight,    &r.earliestMorning};
  std::vector<uint16_t> indices;
  for (const auto* ref : refs) indices.push_back(ref->book == stats::NO_BOOK ? 0 : ref->book);
  std::vector<std::string> titles;
  READING_STATS.bookTitles(indices, titles);
  const auto title = [&titles](const size_t i) {
    return i < titles.size() && !titles[i].empty() ? titles[i].c_str() : tr(STR_STATS_UNKNOWN_BOOK);
  };

  char text[32];
  statstext::duration(r.longestSession.value, text, sizeof(text));
  setBookRecord(ROW_LONGEST_SESSION, r.longestSession, text, title(0));
  statstext::duration(r.longestSitting.value, text, sizeof(text));
  setBookRecord(ROW_LONGEST_SITTING, r.longestSitting, text, title(1));
  snprintf(text, sizeof(text), tr(STR_STATS_PAGES_PER_HOUR), static_cast<unsigned>(r.fastestBook.value));
  setBookRecord(ROW_FASTEST, r.fastestBook, text, title(2));
  snprintf(text, sizeof(text), "%u", static_cast<unsigned>(r.mostLookups.value));
  setBookRecord(ROW_MOST_LOOKUPS, r.mostLookups, text, title(3));
  statstext::clockTime(static_cast<int>(r.latestNight.value), text, sizeof(text));
  setBookRecord(ROW_LATEST, r.latestNight, text, title(4));
  statstext::clockTime(static_cast<int>(r.earliestMorning.value), text, sizeof(text));
  setBookRecord(ROW_EARLIEST, r.earliestMorning, text, title(5));

  for (const int row : {ROW_MOST_DAY, ROW_MOST_WEEK, ROW_STREAK, ROW_LOOKUPS_PER_HOUR, ROW_LATE}) {
    rows[row].value = values[row];
    snprintf(values[row], sizeof(values[row]), "-");
  }
  if (r.mostDaySec > 0) {
    statstext::duration(r.mostDaySec, values[ROW_MOST_DAY], sizeof(values[ROW_MOST_DAY]));
    statstext::dayLong(r.mostDay, true, subtitles[ROW_MOST_DAY], sizeof(subtitles[ROW_MOST_DAY]));
    rows[ROW_MOST_DAY].subtitle = subtitles[ROW_MOST_DAY];
  }
  if (r.mostWeekSec > 0) {
    char day[32];
    statstext::duration(r.mostWeekSec, values[ROW_MOST_WEEK], sizeof(values[ROW_MOST_WEEK]));
    statstext::dayLong(stats::weekStartDay(r.mostWeek), true, day, sizeof(day));
    snprintf(subtitles[ROW_MOST_WEEK], sizeof(subtitles[ROW_MOST_WEEK]), tr(STR_STATS_WEEK_OF), day);
    rows[ROW_MOST_WEEK].subtitle = subtitles[ROW_MOST_WEEK];
  }
  if (r.longestStreak > 0) {
    if (r.longestStreak == 1) {
      snprintf(values[ROW_STREAK], sizeof(values[ROW_STREAK]), "%s", tr(STR_STATS_ONE_DAY));
    } else {
      snprintf(values[ROW_STREAK], sizeof(values[ROW_STREAK]), tr(STR_STATS_DAYS_COUNT), r.longestStreak);
    }
  }
  if (r.activeSec > 0) {
    statstext::tenths(stats::lookupsPerHourTenths(r.lookups, r.activeSec), values[ROW_LOOKUPS_PER_HOUR],
                      sizeof(values[ROW_LOOKUPS_PER_HOUR]));
    if (r.lookups == 1) {
      snprintf(subtitles[ROW_LOOKUPS_PER_HOUR], sizeof(subtitles[ROW_LOOKUPS_PER_HOUR]), "%s", tr(STR_STATS_ONE_LOOKUP));
    } else {
      snprintf(subtitles[ROW_LOOKUPS_PER_HOUR], sizeof(subtitles[ROW_LOOKUPS_PER_HOUR]), tr(STR_STATS_LOOKUPS_COUNT),
               static_cast<unsigned>(r.lookups));
    }
    rows[ROW_LOOKUPS_PER_HOUR].subtitle = subtitles[ROW_LOOKUPS_PER_HOUR];
    statstext::sessionCount(r.lateSessions, values[ROW_LATE], sizeof(values[ROW_LATE]));
    statstext::lateDetail(r.lateSec, subtitles[ROW_LATE], sizeof(subtitles[ROW_LATE]));
    rows[ROW_LATE].subtitle = subtitles[ROW_LATE];
  }
}

void ReadingStatsRecordsActivity::activateIndex(const int index) {
  if (index < 0 || index >= ROW_COUNT || rowBook[index] == stats::NO_BOOK) return;
  app.clearTapFlash();
  ReadingStatsBookActivity::push(*this, renderer, mappedInput, rowBook[index]);
}

void ReadingStatsRecordsActivity::drawFooter() {
  const int index = nav.selected;
  const bool book = index >= 0 && index < ROW_COUNT && rowBook[index] != stats::NO_BOOK;
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), book ? tr(STR_OPEN) : "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void ReadingStatsRecordsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  fui::ListProps props;
  props.items = rows;
  props.count = ROW_COUNT;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}
