#include "ReadingStatsCompareActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "StatsText.h"
#include "components/UITheme.h"
#include "stats/ReadingStats.h"
#include "stats/StatsQueries.h"

namespace fui = freeink::ui;

ReadingStatsCompareActivity::ReadingStatsCompareActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                         const uint16_t first, const uint16_t second)
    : UiListActivity("ReadingStatsCompare", renderer, mappedInput), books{first, second} {}

const char* ReadingStatsCompareActivity::headerTitle() const { return tr(STR_STATS_COMPARE_TITLE); }

void ReadingStatsCompareActivity::onEnter() {
  char values[2][ROW_COUNT][24] = {};
  for (int b = 0; b < 2; ++b) {
    if (!READING_STATS.bookTitle(books[b], titles[b], sizeof(titles[b]))) {
      snprintf(titles[b], sizeof(titles[b]), "%s", tr(STR_STATS_UNKNOWN_BOOK));
    }
    // Totals and style only: no session rows are kept.
    stats::BookDetail detail;
    stats::loadBookDetail(books[b], detail, false);
    const stats::BookTotals& t = detail.agg.totals;
    statstext::compactDuration(t.totalActiveSec, values[b][ROW_TOTAL], sizeof(values[b][ROW_TOTAL]));
    const uint32_t speed = stats::pagesPerHour(t.charsRead, t.learnedMs);
    if (speed > 0) {
      snprintf(values[b][ROW_SPEED], sizeof(values[b][ROW_SPEED]), tr(STR_STATS_PAGES_PER_HOUR),
               static_cast<unsigned>(speed));
    } else {
      snprintf(values[b][ROW_SPEED], sizeof(values[b][ROW_SPEED]), "-");
    }
    snprintf(values[b][ROW_SESSIONS], sizeof(values[b][ROW_SESSIONS]), "%u", static_cast<unsigned>(t.sessions));
    snprintf(values[b][ROW_SITTINGS], sizeof(values[b][ROW_SITTINGS]), "%u", static_cast<unsigned>(t.sittings));
    snprintf(values[b][ROW_STYLE], sizeof(values[b][ROW_STYLE]), "%s", statstext::styleName(detail.style));
    const int days = stats::daysToFinish(detail.agg);
    if (days > 0) {
      snprintf(values[b][ROW_DAYS], sizeof(values[b][ROW_DAYS]), "%d", days);
    } else {
      snprintf(values[b][ROW_DAYS], sizeof(values[b][ROW_DAYS]), "-");
    }
  }
  for (int i = 0; i < ROW_COUNT; ++i) rows[i].actionValue = static_cast<int16_t>(i);
  rows[ROW_FIRST].label = titles[0];
  rows[ROW_FIRST].value = "1";
  rows[ROW_SECOND].label = titles[1];
  rows[ROW_SECOND].value = "2";
  rows[ROW_TOTAL].label = tr(STR_STATS_TOTAL);
  rows[ROW_SPEED].label = tr(STR_STATS_SPEED);
  rows[ROW_SESSIONS].label = tr(STR_STATS_SESSIONS);
  rows[ROW_SITTINGS].label = tr(STR_STATS_SITTINGS);
  rows[ROW_STYLE].label = tr(STR_STATS_STYLE);
  rows[ROW_DAYS].label = tr(STR_STATS_DAYS_TO_FINISH);
  for (int i = ROW_TOTAL; i < ROW_COUNT; ++i) {
    snprintf(rowValues[i], sizeof(rowValues[i]), "%s | %s", values[0][i], values[1][i]);
    rows[i].value = rowValues[i];
  }
  UiListActivity::onEnter();
}

void ReadingStatsCompareActivity::drawFooter() {
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void ReadingStatsCompareActivity::buildScreen(UiScreen& screen) {
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
