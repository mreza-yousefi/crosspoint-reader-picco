#include "ReadingStatsDayActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "StatsText.h"
#include "components/UITheme.h"
#include "stats/ReadingStats.h"
#include "stats/StatsLogic.h"
#include "stats/StatsQueries.h"

namespace fui = freeink::ui;

namespace {
int localMinuteOfDay(const uint32_t utc, const int16_t tzOffsetMin) {
  const int64_t local = static_cast<int64_t>(utc) + static_cast<int64_t>(tzOffsetMin) * 60;
  const int64_t secondsOfDay = local - static_cast<int64_t>(stats::localDay(utc, tzOffsetMin)) * 86400;
  return static_cast<int>(secondsOfDay / 60);
}
}  // namespace

ReadingStatsDayActivity::ReadingStatsDayActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                 const int32_t day)
    : UiListActivity("ReadingStatsDay", renderer, mappedInput), day(day) {}

void ReadingStatsDayActivity::onEnter() {
  statstext::dayLong(day, true, title, sizeof(title));

  std::vector<stats::DaySession> sessions;
  stats::loadDaySessions(day, sessions);
  labels.clear();
  subtitles.clear();
  values.clear();
  items.clear();
  labels.reserve(sessions.size());
  subtitles.reserve(sessions.size());
  values.reserve(sessions.size());
  items.reserve(sessions.size());

  // Titles repeat across a day's sessions: read each book's title once.
  int lastBook = -1;
  std::string lastTitle;
  for (const auto& entry : sessions) {
    const stats::SessionRecord& s = entry.record;
    if (s.bookIndex != lastBook) {
      char bookTitle[sizeof(stats::BookRecord::title)];
      lastBook = s.bookIndex;
      lastTitle = READING_STATS.bookTitle(s.bookIndex, bookTitle, sizeof(bookTitle))
                      ? std::string(bookTitle)
                      : std::string(tr(STR_STATS_UNKNOWN_BOOK));
    }
    labels.push_back(lastTitle);

    char start[16];
    char end[16];
    char line[64];
    const int startMinute = localMinuteOfDay(s.startUtc, s.tzOffsetMin);
    statstext::clockTime(startMinute, start, sizeof(start));
    if (entry.live) {
      snprintf(line, sizeof(line), "%s - %s", start, tr(STR_STATS_READING_NOW));
    } else {
      statstext::clockTime(localMinuteOfDay(s.startUtc + s.durationSec, s.tzOffsetMin), end, sizeof(end));
      snprintf(line, sizeof(line), "%s - %s", start, end);
    }
    subtitles.emplace_back(line);
    statstext::duration(s.activeSec, line, sizeof(line));
    values.emplace_back(line);
  }
  for (size_t i = 0; i < labels.size(); ++i) {
    fui::ListItem item;
    item.label = labels[i].c_str();
    item.subtitle = subtitles[i].c_str();
    item.value = values[i].c_str();
    item.actionValue = static_cast<int16_t>(i);
    items.push_back(item);
  }
  UiListActivity::onEnter();
}

void ReadingStatsDayActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  if (items.empty()) {
    screen.centeredText(tr(STR_STATS_NO_READING_DAY));
    return;
  }
  fui::ListProps props;
  props.items = items.data();
  props.count = static_cast<uint16_t>(items.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}
