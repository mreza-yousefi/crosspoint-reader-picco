#include "ReadingStatsDayActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "ReadingStatsBookActivity.h"
#include "StatsText.h"
#include "components/UITheme.h"
#include "stats/ReadingStats.h"
#include "stats/StatsLogic.h"
#include "stats/StatsQueries.h"

namespace fui = freeink::ui;

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
  books.clear();
  labels.reserve(sessions.size());
  books.reserve(sessions.size());
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
    books.push_back(s.bookIndex);

    char start[16];
    char end[16];
    char line[96];
    statstext::clockTime(statstext::minuteOfDay(s.startUtc, s.tzOffsetMin), start, sizeof(start));
    int len;
    if (entry.live) {
      len = snprintf(line, sizeof(line), "%s - %s", start, tr(STR_STATS_READING_NOW));
    } else {
      statstext::clockTime(statstext::minuteOfDay(s.startUtc + s.durationSec, s.tzOffsetMin), end, sizeof(end));
      len = snprintf(line, sizeof(line), "%s - %s", start, end);
    }
    if (s.dictSec > 0 && len > 0 && static_cast<size_t>(len) < sizeof(line)) {
      char dict[24];
      char text[40];
      statstext::duration(s.dictSec, dict, sizeof(dict));
      snprintf(text, sizeof(text), tr(STR_STATS_DICTIONARY_SHORT), dict);
      snprintf(line + len, sizeof(line) - len, " · %s", text);
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

void ReadingStatsDayActivity::activateIndex(const int index) {
  if (index < 0 || index >= static_cast<int>(books.size())) return;
  app.clearTapFlash();
  ReadingStatsBookActivity::push(*this, renderer, mappedInput, books[index]);
}

void ReadingStatsDayActivity::drawFooter() {
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), items.empty() ? "" : tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
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
