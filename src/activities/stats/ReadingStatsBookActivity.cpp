#include "ReadingStatsBookActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "ReadingStatsBooksActivity.h"
#include "StatsText.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "stats/ReadingStats.h"

namespace fui = freeink::ui;

ReadingStatsBookActivity::ReadingStatsBookActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                   const uint16_t bookIndex)
    : UiTabListActivity("ReadingStatsBook", renderer, mappedInput), bookIndex(bookIndex) {
  tabPillMaxPad = 8;
}

bool ReadingStatsBookActivity::push(Activity& parent, GfxRenderer& renderer, MappedInputManager& mappedInput,
                                    const int bookIndex) {
  if (bookIndex < 0) return false;
  auto screen = makeUniqueNoThrow<ReadingStatsBookActivity>(renderer, mappedInput, static_cast<uint16_t>(bookIndex));
  if (!screen) {
    LOG_ERR("STATS", "OOM: book stats");
    return false;
  }
  parent.startActivityForResult(std::move(screen), [](const ActivityResult&) {});
  return true;
}

void ReadingStatsBookActivity::onEnter() {
  // Opened over a book from its menu: not reading time.
  READING_STATS.setPaused(true);
  UiTabListActivity::onEnter();
  for (int i = 0; i < ROW_COUNT; ++i) rows[i].actionValue = static_cast<int16_t>(i);
  rows[ROW_TOTAL].label = tr(STR_STATS_TOTAL);
  rows[ROW_SESSIONS].label = tr(STR_STATS_SESSIONS);
  rows[ROW_SITTINGS].label = tr(STR_STATS_SITTINGS);
  rows[ROW_AVERAGE].label = tr(STR_STATS_AVERAGE_SESSION);
  rows[ROW_FIRST].label = tr(STR_STATS_FIRST_READ);
  rows[ROW_LAST].label = tr(STR_STATS_LAST_READ);
  rows[ROW_STYLE].label = tr(STR_STATS_STYLE);
  rows[ROW_SPEED].label = tr(STR_STATS_SPEED);
  rows[ROW_POSITION].label = tr(STR_STATS_PROGRESS);
  rows[ROW_LOOKUPS].label = tr(STR_STATS_LOOKUPS);
  rows[ROW_DICTIONARY].label = tr(STR_STATS_DICTIONARY_TIME);
  rows[ROW_HIDDEN].label = tr(STR_HIDE_FROM_STATS);
  rows[ROW_COMPARE].label = tr(STR_STATS_COMPARE);
  {
    RenderLock lock;
    if (READING_STATS.needsLogRebuild()) GUI.drawPopup(renderer, tr(STR_STATS_UPDATING));
    READING_STATS.prepareSummaries();
    load();
  }
  requestUpdate();
}

void ReadingStatsBookActivity::load() {
  if (!READING_STATS.bookTitle(bookIndex, title, sizeof(title))) {
    snprintf(title, sizeof(title), "%s", tr(STR_STATS_UNKNOWN_BOOK));
  }
  stats::loadBookDetail(bookIndex, detail);
  formatSummary();
}

void ReadingStatsBookActivity::formatSummary() {
  const stats::BookTotals& t = detail.agg.totals;
  const auto set = [this](const int row, const char* text) {
    snprintf(rowValues[row], sizeof(rowValues[row]), "%s", text);
  };
  const auto setDuration = [this](const int row, const uint32_t seconds) {
    statstext::duration(seconds, rowValues[row], sizeof(rowValues[row]));
  };
  // Local days by each session's own offset, as in the day list.
  const auto setDay = [this, &set](const int row, const bool known, const int32_t day) {
    if (known) {
      statstext::dayLong(day, true, rowValues[row], sizeof(rowValues[row]));
    } else {
      set(row, "-");
    }
  };
  const stats::StyleAcc& dated = detail.agg.style;
  setDuration(ROW_TOTAL, t.totalActiveSec);
  snprintf(rowValues[ROW_SESSIONS], sizeof(rowValues[ROW_SESSIONS]), "%u", static_cast<unsigned>(t.sessions));
  snprintf(rowValues[ROW_SITTINGS], sizeof(rowValues[ROW_SITTINGS]), "%u", static_cast<unsigned>(t.sittings));
  if (t.sessions > 0) {
    setDuration(ROW_AVERAGE, t.totalActiveSec / t.sessions);
  } else {
    set(ROW_AVERAGE, "-");
  }
  setDay(ROW_FIRST, dated.sessions > 0, dated.firstDay);
  setDay(ROW_LAST, dated.sessions > 0, dated.lastDay);
  set(ROW_STYLE, statstext::styleName(detail.style));
  const uint32_t speed = stats::pagesPerHour(t.charsRead, t.learnedMs);
  if (speed > 0) {
    snprintf(rowValues[ROW_SPEED], sizeof(rowValues[ROW_SPEED]), tr(STR_STATS_PAGES_PER_HOUR),
             static_cast<unsigned>(speed));
  } else {
    set(ROW_SPEED, "-");
  }
  if (t.finishedUtc != 0) {
    rows[ROW_FINISH].label = tr(STR_STATS_FINISHED);
    if (detail.agg.hasFinishedDay) {
      setDay(ROW_FINISH, true, detail.agg.finishedDay);
    } else {
      set(ROW_FINISH, tr(STR_YES));
    }
  } else {
    rows[ROW_FINISH].label = tr(STR_STATS_TIME_TO_FINISH);
    const uint32_t left = stats::secondsToFinish(detail.agg);
    if (left > 0) {
      setDuration(ROW_FINISH, left);
    } else {
      set(ROW_FINISH, "-");
    }
  }
  snprintf(rowValues[ROW_POSITION], sizeof(rowValues[ROW_POSITION]), "%u%%",
           static_cast<unsigned>(std::min<uint16_t>(t.lastPercentBp, stats::END_OF_BOOK_BP) / 100));
  snprintf(rowValues[ROW_LOOKUPS], sizeof(rowValues[ROW_LOOKUPS]), "%u", static_cast<unsigned>(detail.agg.lookups));
  setDuration(ROW_DICTIONARY, detail.agg.dictSec);
  for (int i = 0; i < ROW_HIDDEN; ++i) rows[i].value = rowValues[i];
}

int ReadingStatsBookActivity::listCount() const {
  switch (tab) {
    case TAB_SUMMARY:
      return ROW_COUNT;
    case TAB_SESSIONS:
      return static_cast<int>(detail.sessions.size());
    default:
      return 0;
  }
}

const char* ReadingStatsBookActivity::tabLabel(const int index) const {
  switch (index) {
    case TAB_PROGRESS:
      return tr(STR_STATS_PROGRESS);
    case TAB_SESSIONS:
      return tr(STR_STATS_SESSIONS);
    default:
      return tr(STR_STATS_SUMMARY);
  }
}

void ReadingStatsBookActivity::selectTab(const int index) {
  if (index < 0 || index >= TAB_COUNT) return;
  {
    RenderLock lock;
    tab = index;
  }
  activeNav().selected = 0;
  activeNav().top = 0;
  requestUpdate();
}

void ReadingStatsBookActivity::onTabAction(const int index) {
  app.clearTapFlash();
  selectTab(index);
}

void ReadingStatsBookActivity::stepTab(const int direction) {
  selectTab((tab + (direction > 0 ? 1 : TAB_COUNT - 1)) % TAB_COUNT);
}

void ReadingStatsBookActivity::activateIndex(const int index) {
  if (tab != TAB_SUMMARY) return;
  if (index == ROW_HIDDEN) {
    toggleHidden();
  } else if (index == ROW_COMPARE) {
    openCompare();
  }
}

void ReadingStatsBookActivity::toggleHidden() {
  {
    RenderLock lock;
    GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
    if (!READING_STATS.setBookHidden(bookIndex, !detail.hidden)) LOG_ERR("STATS", "Could not change the hidden flag");
    load();
  }
  requestUpdate();
}

void ReadingStatsBookActivity::openCompare() {
  app.clearTapFlash();
  auto picker = makeUniqueNoThrow<ReadingStatsBooksActivity>(renderer, mappedInput,
                                                             ReadingStatsBooksActivity::Mode::Pick, bookIndex);
  if (!picker) {
    LOG_ERR("STATS", "OOM: book picker");
    return;
  }
  startActivityForResult(std::move(picker), [this](const ActivityResult&) { requestUpdate(); });
}

bool ReadingStatsBookActivity::handleButtons() {
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

void ReadingStatsBookActivity::drawFooter() {
  const int ring = ringPos();
  const char* confirm = "";
  if (ring == 0 || (tab == TAB_SUMMARY && ring - 1 == ROW_HIDDEN)) {
    confirm = tr(STR_TOGGLE);
  } else if (tab == TAB_SUMMARY && ring - 1 == ROW_COMPARE) {
    confirm = tr(STR_OPEN);
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirm, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void ReadingStatsBookActivity::provideSessionRow(void* ctx, const uint16_t index, fui::ListItem& item) {
  auto* self = static_cast<ReadingStatsBookActivity*>(ctx);
  if (index >= self->detail.sessions.size()) return;
  const stats::BookSessionRow& s = self->detail.sessions[index];
  if (s.startUtc != 0 && (s.flags & stats::SESSION_WALL_KNOWN)) {
    char day[32];
    char time[16];
    statstext::dayLong(stats::localDay(s.startUtc, s.tzOffsetMin), false, day, sizeof(day));
    statstext::clockTime(statstext::minuteOfDay(s.startUtc, s.tzOffsetMin), time, sizeof(time));
    snprintf(self->rowLabel, sizeof(self->rowLabel), "%s, %s", day, time);
  } else {
    snprintf(self->rowLabel, sizeof(self->rowLabel), "%s", tr(STR_STATS_TIME_UNKNOWN));
  }
  int len = snprintf(self->rowSubtitle, sizeof(self->rowSubtitle), "%u%% - %u%%",
                     static_cast<unsigned>(std::min<uint16_t>(s.startPercentBp, stats::END_OF_BOOK_BP) / 100),
                     static_cast<unsigned>(std::min<uint16_t>(s.endPercentBp, stats::END_OF_BOOK_BP) / 100));
  if (s.dictSec > 0 && len > 0 && static_cast<size_t>(len) < sizeof(self->rowSubtitle)) {
    char dict[24];
    char text[40];
    statstext::duration(s.dictSec, dict, sizeof(dict));
    snprintf(text, sizeof(text), tr(STR_STATS_DICTIONARY_SHORT), dict);
    len += snprintf(self->rowSubtitle + len, sizeof(self->rowSubtitle) - len, " · %s", text);
  }
  if (s.live && len > 0 && static_cast<size_t>(len) < sizeof(self->rowSubtitle)) {
    snprintf(self->rowSubtitle + len, sizeof(self->rowSubtitle) - len, " · %s", tr(STR_STATS_READING_NOW));
  }
  statstext::duration(s.activeSec, self->rowValue, sizeof(self->rowValue));
  item.label = self->rowLabel;
  item.subtitle = self->rowSubtitle;
  item.value = self->rowValue;
  item.actionValue = static_cast<int16_t>(index);
}

void ReadingStatsBookActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int rightInset = renderer.getScreenWidth() - (safe.x + safe.width);
  const int bottomInset = renderer.getScreenHeight() - (safe.y + safe.height);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), static_cast<int16_t>(rightInset),
                  static_cast<int16_t>(bottomInset + metrics.buttonHintsHeight), static_cast<int16_t>(safe.x)});
  buildTabBar(screen);

  if (tab == TAB_PROGRESS) {
    drawProgress(screen, screen.body());
    return;
  }
  fui::ListProps props;
  if (tab == TAB_SUMMARY) {
    GUI.setCheckboxRow(rows[ROW_HIDDEN], detail.hidden);
    props.items = rows;
    props.count = ROW_COUNT;
  } else {
    if (detail.sessions.empty()) {
      screen.centeredText(tr(STR_STATS_NO_BOOKS));
      return;
    }
    props.rowProvider = &ReadingStatsBookActivity::provideSessionRow;
    props.rowProviderCtx = this;
    props.count = static_cast<uint16_t>(detail.sessions.size());
  }
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncTabListViewport(screen, props);
  screen.list(props);
}

// Position at the end of each reading day, from the first to the last, as columns.
void ReadingStatsBookActivity::drawProgress(UiScreen& screen, const fui::Rect area) {
  const auto& points = detail.progress;
  if (points.empty()) {
    screen.centeredText(tr(STR_STATS_NO_DATED_READING));
    return;
  }
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int side = metrics.contentSidePadding;
  const int x0 = area.x + side;
  const int width = area.width - 2 * side;
  const int bottom = area.y + area.height;
  const int smallHeight = renderer.getLineHeight(SMALL_FONT_ID);
  int y = area.y + 8;

  const int32_t firstDay = points.front().day;
  const int32_t lastDay = points.back().day;
  char first[24];
  char last[24];
  char text[64];
  statstext::dayShort(firstDay, first, sizeof(first));
  statstext::dayShort(lastDay, last, sizeof(last));
  snprintf(text, sizeof(text), tr(STR_STATS_PROGRESS_RANGE), first, last);
  const std::string info = renderer.truncatedText(UI_12_FONT_ID, text, width, EpdFontFamily::BOLD);
  renderer.drawText(UI_12_FONT_ID,
                    x0 + (width - renderer.getTextWidth(UI_12_FONT_ID, info.c_str(), EpdFontFamily::BOLD)) / 2, y,
                    info.c_str(), true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 8 + smallHeight / 2;

  const int columns = static_cast<int>(std::min<int64_t>(MAX_COLUMNS, static_cast<int64_t>(lastDay) - firstDay + 1));
  uint16_t values[MAX_COLUMNS];
  stats::progressColumns(points.data(), points.size(), firstDay, lastDay, columns, values);

  const int axisWidth = renderer.getTextWidth(SMALL_FONT_ID, "100%") + 6;
  const int plotTop = y;
  const int plotBottom = bottom - smallHeight - 6;
  const int plotHeight = plotBottom - plotTop;
  const int slot = (width - axisWidth) / columns;
  if (plotHeight < 20 || slot < 1) return;
  const int plotWidth = slot * columns;
  const int plotX = x0 + width - plotWidth;
  for (int percent = 25; percent <= 100; percent += 25) {
    const int gy = plotBottom - percent * plotHeight / 100;
    for (int gx = plotX; gx < plotX + plotWidth; gx += 4) renderer.drawPixel(gx, gy, true);
    char label[8];
    snprintf(label, sizeof(label), "%d%%", percent);
    renderer.drawText(SMALL_FONT_ID, plotX - 4 - renderer.getTextWidth(SMALL_FONT_ID, label), gy - smallHeight / 2,
                      label, true);
  }
  const int barWidth = slot > 3 ? slot - 1 : slot;
  for (int c = 0; c < columns; ++c) {
    const int height = static_cast<int>(static_cast<int32_t>(std::min<uint16_t>(values[c], stats::END_OF_BOOK_BP)) *
                                        plotHeight / stats::END_OF_BOOK_BP);
    if (height > 0) renderer.fillRect(plotX + c * slot, plotBottom - height, barWidth, height, true);
  }
  renderer.drawLine(plotX, plotBottom, plotX + plotWidth - 1, plotBottom, true);
  // The first and last day under the axis ends.
  renderer.drawText(SMALL_FONT_ID, plotX, plotBottom + 4, first, true);
  const int lastWidth = renderer.getTextWidth(SMALL_FONT_ID, last);
  if (columns > 1 && plotX + plotWidth - lastWidth > plotX + renderer.getTextWidth(SMALL_FONT_ID, first) + 8) {
    renderer.drawText(SMALL_FONT_ID, plotX + plotWidth - lastWidth, plotBottom + 4, last, true);
  }
}
