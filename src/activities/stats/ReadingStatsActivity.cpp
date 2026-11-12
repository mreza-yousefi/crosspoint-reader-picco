#include "ReadingStatsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "ReadingStatsDayActivity.h"
#include "StatsText.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "stats/ReadingStats.h"

namespace fui = freeink::ui;
using stats::Range;

namespace {
// Gridline steps, in seconds; the chart uses the first giving at most MAX_GRIDLINES lines.
constexpr uint32_t GRID_STEPS[] = {300,      600,       900,       1800,      3600,       2 * 3600,   3 * 3600,
                                   5 * 3600, 10 * 3600, 20 * 3600, 50 * 3600, 100 * 3600, 200 * 3600, 500 * 3600};
constexpr uint32_t MAX_GRIDLINES = 4;
constexpr int ARROW_WIDTH = 48;

uint32_t sumDays(const std::vector<stats::DayRecord>& days, const int32_t from, const int32_t to) {
  uint32_t total = 0;
  for (const auto& d : days) {
    if (d.day >= from && d.day <= to) total += d.activeSec;
  }
  return total;
}
}  // namespace

ReadingStatsActivity::ReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                           const bool fromReader)
    : UiTabListActivity("ReadingStats", renderer, mappedInput), fromReader(fromReader) {
  tabPillMaxPad = 8;
}

void ReadingStatsActivity::onEnter() {
  // Time on this screen is not reading time for the book underneath.
  READING_STATS.setPaused(true);
  UiTabListActivity::onEnter();
  app.on(ACTION_CHART, &ReadingStatsActivity::chartTrampoline, this);
  app.on(ACTION_PAGE, &ReadingStatsActivity::pageTrampoline, this);
  for (int i = 0; i < ROW_COUNT; ++i) rows[i].actionValue = static_cast<int16_t>(i);
  rows[ROW_TODAY].label = tr(STR_TODAY);
  rows[ROW_WEEK].label = tr(STR_STATS_THIS_WEEK);
  rows[ROW_MONTH].label = tr(STR_STATS_THIS_MONTH);
  rows[ROW_YEAR].label = tr(STR_STATS_THIS_YEAR);
  rows[ROW_TOTAL].label = tr(STR_STATS_TOTAL);
  rows[ROW_STREAK].label = tr(STR_STATS_CURRENT_STREAK);
  rows[ROW_LONGEST].label = tr(STR_STATS_LONGEST_STREAK);
  rows[ROW_FINISHED].label = tr(STR_STATS_FINISHED_THIS_YEAR);
  rows[ROW_IGNORE_SHORT].label = tr(STR_STATS_IGNORE_SHORT_OPENS);
  RenderLock lock;
  READING_STATS.prepareSummaries();
  loadData();
}

void ReadingStatsActivity::loadData() {
  stats::loadDayTotals(data);
  todayDay = stats::today(clockKnown);
  // Without a trusted clock the charts end at the last day with reading.
  if (!clockKnown && !data.days.empty()) todayDay = data.days.back().day;
  finishedThisYear = clockKnown ? stats::countFinishedInYear(stats::yearOf(todayDay)) : 0;
  streaks = stats::computeStreaks(data.days.data(), data.days.size(), todayDay);
  computeOverview();
  if (chartTab()) computeBars();
}

void ReadingStatsActivity::computeOverview() {
  const auto setDuration = [this](const int row, const uint32_t seconds) {
    statstext::duration(seconds, rowValues[row], sizeof(rowValues[row]));
  };
  const auto setDays = [this](const int row, const int days) {
    if (days == 1) {
      snprintf(rowValues[row], sizeof(rowValues[row]), "%s", tr(STR_STATS_ONE_DAY));
    } else {
      snprintf(rowValues[row], sizeof(rowValues[row]), tr(STR_STATS_DAYS_COUNT), days);
    }
  };
  if (clockKnown) {
    setDuration(ROW_TODAY, sumDays(data.days, todayDay, todayDay));
    setDuration(ROW_WEEK, sumDays(data.days, stats::weekStartDay(stats::weekIndex(todayDay)), todayDay));
    int year;
    unsigned month, dayOfMonth;
    stats::civilFromDays(todayDay, year, month, dayOfMonth);
    setDuration(ROW_MONTH, sumDays(data.days, stats::daysFromCivil(year, month, 1), todayDay));
    setDuration(ROW_YEAR, sumDays(data.days, stats::daysFromCivil(year, 1, 1), todayDay));
    setDays(ROW_STREAK, streaks.current);
    snprintf(rowValues[ROW_FINISHED], sizeof(rowValues[ROW_FINISHED]), "%d", finishedThisYear);
    rows[ROW_TODAY].subtitle = nullptr;
  } else {
    for (const int row : {ROW_TODAY, ROW_WEEK, ROW_MONTH, ROW_YEAR, ROW_STREAK, ROW_FINISHED}) {
      snprintf(rowValues[row], sizeof(rowValues[row]), "-");
    }
    rows[ROW_TODAY].subtitle = tr(STR_STATS_CLOCK_UNKNOWN);
  }
  setDuration(ROW_TOTAL, sumDays(data.days, INT32_MIN, INT32_MAX) + data.unknownTimeSec);
  setDays(ROW_LONGEST, streaks.longest);
  for (int i = 0; i < ROW_IGNORE_SHORT; ++i) rows[i].value = rowValues[i];
}

Range ReadingStatsActivity::range() const {
  switch (tab) {
    case TAB_WEEKS:
      return Range::Week;
    case TAB_MONTHS:
      return Range::Month;
    case TAB_YEARS:
      return Range::Year;
    default:
      return Range::Day;
  }
}

void ReadingStatsActivity::computeBars() {
  const Range r = range();
  const int32_t anchor = stats::bucketOf(r, todayDay);
  switch (r) {
    case Range::Day:
      barCount = 30;
      break;
    case Range::Week:
      barCount = 26;
      break;
    case Range::Month:
      barCount = 12;
      break;
    case Range::Year: {
      const int32_t firstYear = data.days.empty() ? anchor : stats::yearOf(data.days.front().day);
      barCount = std::clamp<int>(anchor - firstYear + 1, 5, MAX_YEAR_BARS);
      break;
    }
  }
  firstBucket = anchor - (barCount - 1) - pageBack[tab] * barCount;
  stats::bucketActive(data.days.data(), data.days.size(), r, firstBucket, barCount, bars);
}

bool ReadingStatsActivity::canPageOlder() const {
  return !data.days.empty() && firstBucket > stats::bucketOf(range(), data.days.front().day);
}

int ReadingStatsActivity::focusedBar() const {
  const int ring = ringPos();
  return ring > 0 && ring <= barCount ? ring - 1 : barCount - 1;
}

int ReadingStatsActivity::listCount() const { return chartTab() ? barCount : ROW_COUNT; }

const char* ReadingStatsActivity::headerTitle() const { return tr(STR_READING_STATS); }

const char* ReadingStatsActivity::tabLabel(const int index) const {
  switch (index) {
    case TAB_DAYS:
      return tr(STR_STATS_DAYS);
    case TAB_WEEKS:
      return tr(STR_STATS_WEEKS);
    case TAB_MONTHS:
      return tr(STR_STATS_MONTHS);
    case TAB_YEARS:
      return tr(STR_STATS_YEARS);
    default:
      return tr(STR_STATS_OVERVIEW);
  }
}

void ReadingStatsActivity::selectTab(const int index) {
  if (index < 0 || index >= TAB_COUNT) return;
  {
    RenderLock lock;
    tab = index;
    if (chartTab()) computeBars();
  }
  activeNav().selected = 0;
  activeNav().top = 0;
  requestUpdate();
}

void ReadingStatsActivity::onTabAction(const int index) {
  app.clearTapFlash();
  selectTab(index);
}

void ReadingStatsActivity::stepTab(const int direction) {
  selectTab((tab + (direction > 0 ? 1 : TAB_COUNT - 1)) % TAB_COUNT);
}

void ReadingStatsActivity::pageBy(const int direction) {
  if (direction > 0 && !canPageOlder()) return;
  if (direction < 0 && pageBack[tab] == 0) return;
  RenderLock lock;
  pageBack[tab] += direction > 0 ? 1 : -1;
  computeBars();
}

void ReadingStatsActivity::chartStep(const int direction) {
  const int ring = ringPos();
  if (ring == 0 || barCount == 0) {
    moveRingTo(barCount);  // from the tabs to the newest bar
    return;
  }
  int bar = ring - 1 + direction;
  if (bar < 0) {
    if (!canPageOlder()) return;
    pageBy(1);
    bar = barCount - 1;
  } else if (bar >= barCount) {
    if (pageBack[tab] == 0) {
      moveRingTo(0);
      return;
    }
    pageBy(-1);
    bar = 0;
  }
  moveRingTo(bar + 1);
}

void ReadingStatsActivity::chartTrampoline(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<ReadingStatsActivity*>(user);
  if (event.dragPermille < 0 || self->barCount == 0) return;
  const int bar = std::clamp(event.dragPermille * self->barCount / 1000, 0, self->barCount - 1);
  self->app.clearTapFlash();
  self->activeNav().selected = bar + 1;
  self->activateIndex(bar);
}

void ReadingStatsActivity::pageTrampoline(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<ReadingStatsActivity*>(user);
  self->app.clearTapFlash();
  self->pageBy(event.value);
  self->requestUpdate();
}

void ReadingStatsActivity::activateIndex(const int index) {
  if (!chartTab()) {
    if (index == ROW_IGNORE_SHORT) toggleIgnoreShortOpens();
    return;
  }
  if (tab == TAB_DAYS) {
    openDay(index);
    return;
  }
  moveRingTo(index + 1);
}

void ReadingStatsActivity::toggleIgnoreShortOpens() {
  SETTINGS.statsIgnoreShortOpens = SETTINGS.statsIgnoreShortOpens ? 0 : 1;
  SETTINGS.saveToFile();
  {
    RenderLock lock;
    GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
    READING_STATS.rebuildSummaries();
    loadData();
  }
  requestUpdate();
}

void ReadingStatsActivity::openDay(const int bar) {
  if (bar < 0 || bar >= barCount) return;
  auto day = makeUniqueNoThrow<ReadingStatsDayActivity>(renderer, mappedInput, firstBucket + bar);
  if (!day) {
    LOG_ERR("STATS", "OOM: day list");
    return;
  }
  startActivityForResult(std::move(day), [this](const ActivityResult&) { requestUpdate(); });
}

bool ReadingStatsActivity::handleCustomInput() {
  if (!chartTab()) return false;
  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Right || swipe == MappedInputManager::SwipeDir::Left) {
    // Swiping right pulls older days in from the left.
    pageBy(swipe == MappedInputManager::SwipeDir::Right ? 1 : -1);
    requestUpdate();
    return true;
  }
  return false;
}

bool ReadingStatsActivity::handleButtons() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (fromReader) {
      finish();
    } else {
      onGoHome(HomeMenuItem::STATS);
    }
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

void ReadingStatsActivity::navigateButtons() {
  if (!chartTab()) {
    UiTabListActivity::navigateButtons();
    return;
  }
  buttonNavigator.onNextPress([this] { chartStep(1); });
  buttonNavigator.onPreviousPress([this] { chartStep(-1); });
  buttonNavigator.onNextContinuous([this] {
    activeNav().selected = 0;
    stepTab(1);
  });
  buttonNavigator.onPreviousContinuous([this] {
    activeNav().selected = 0;
    stepTab(-1);
  });
}

void ReadingStatsActivity::drawFooter() {
  const char* confirm = "";
  const int ring = ringPos();
  if (ring == 0) {
    confirm = tr(STR_TOGGLE);
  } else if (!chartTab() && ring - 1 == ROW_IGNORE_SHORT) {
    confirm = tr(STR_TOGGLE);
  } else if (tab == TAB_DAYS) {
    confirm = tr(STR_OPEN);
  }
  const auto labels = chartTab()
                          ? mappedInput.mapLabels(tr(STR_BACK), confirm, tr(STR_STATS_OLDER), tr(STR_STATS_NEWER))
                          : mappedInput.mapLabels(tr(STR_BACK), confirm, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void ReadingStatsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int rightInset = renderer.getScreenWidth() - (safe.x + safe.width);
  const int bottomInset = renderer.getScreenHeight() - (safe.y + safe.height);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), static_cast<int16_t>(rightInset),
                  static_cast<int16_t>(bottomInset + metrics.buttonHintsHeight), static_cast<int16_t>(safe.x)});
  buildTabBar(screen);

  if (chartTab()) {
    if (!clockKnown && data.days.empty()) {
      screen.centeredText(tr(STR_STATS_CLOCK_UNKNOWN));
    } else {
      drawChart(screen, screen.body());
    }
    return;
  }
  GUI.setCheckboxRow(rows[ROW_IGNORE_SHORT], SETTINGS.statsIgnoreShortOpens != 0);
  fui::ListProps props;
  props.items = rows;
  props.count = ROW_COUNT;
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncTabListViewport(screen, props);
  screen.list(props);
}

void ReadingStatsActivity::barTitle(const int bar, char* out, const size_t size) const {
  const int32_t bucket = firstBucket + bar;
  char text[40];
  switch (range()) {
    case Range::Day:
      if (clockKnown && bucket == todayDay) {
        snprintf(text, sizeof(text), "%s", tr(STR_TODAY));
      } else if (clockKnown && bucket == todayDay - 1) {
        snprintf(text, sizeof(text), "%s", tr(STR_YESTERDAY));
      } else {
        statstext::dayLong(bucket, stats::yearOf(bucket) != stats::yearOf(todayDay), text, sizeof(text));
      }
      break;
    case Range::Week: {
      char start[24];
      statstext::dayShort(stats::weekStartDay(bucket), start, sizeof(start));
      snprintf(text, sizeof(text), tr(STR_STATS_WEEK_OF), start);
      break;
    }
    case Range::Month: {
      char month[16];
      statstext::monthName(static_cast<unsigned>(bucket % 12) + 1, month, sizeof(month));
      snprintf(text, sizeof(text), "%s %d", month, static_cast<int>(bucket / 12));
      break;
    }
    case Range::Year:
      snprintf(text, sizeof(text), "%d", static_cast<int>(bucket));
      break;
  }
  char value[32];
  statstext::duration(bars[bar], value, sizeof(value));
  snprintf(out, size, "%s: %s", text, value);
}

bool ReadingStatsActivity::barAxisLabel(const int bar, char* out, const size_t size) const {
  const int32_t bucket = firstBucket + bar;
  switch (range()) {
    case Range::Day: {
      if (stats::weekday(bucket) != 0) return false;  // Mondays
      int year;
      unsigned month, dayOfMonth;
      stats::civilFromDays(bucket, year, month, dayOfMonth);
      snprintf(out, size, "%u", dayOfMonth);
      return true;
    }
    case Range::Week: {
      // The first week starting in each month.
      int year;
      unsigned month, dayOfMonth;
      stats::civilFromDays(stats::weekStartDay(bucket), year, month, dayOfMonth);
      if (dayOfMonth > 7) return false;
      statstext::monthName(month, out, size);
      return true;
    }
    case Range::Month:
      statstext::monthName(static_cast<unsigned>(bucket % 12) + 1, out, size);
      return true;
    case Range::Year:
      snprintf(out, size, "%d", static_cast<int>(bucket));
      return true;
  }
  return false;
}

void ReadingStatsActivity::drawChart(UiScreen& screen, const fui::Rect area) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int side = metrics.contentSidePadding;
  const int x0 = area.x + side;
  const int width = area.width - 2 * side;
  int y = area.y;
  const int bottom = area.y + area.height;
  const int smallHeight = renderer.getLineHeight(SMALL_FONT_ID);
  const int infoFont = UI_12_FONT_ID;
  const int infoHeight = std::max(renderer.getLineHeight(infoFont) + 16, 44);
  if (barCount <= 0 || width <= 0) return;

  // Info line: the focused bar, with arrows to page in time.
  const int focus = focusedBar();
  char text[80];
  barTitle(focus, text, sizeof(text));
  const std::string fitted = renderer.truncatedText(infoFont, text, width - 2 * ARROW_WIDTH, EpdFontFamily::BOLD);
  const int textWidth = renderer.getTextWidth(infoFont, fitted.c_str(), EpdFontFamily::BOLD);
  const int textY = y + (infoHeight - renderer.getLineHeight(infoFont)) / 2;
  renderer.drawText(infoFont, x0 + (width - textWidth) / 2, textY, fitted.c_str(), true, EpdFontFamily::BOLD);
  const int arrowMid = y + infoHeight / 2;
  constexpr int ARROW_HALF = 9;
  if (canPageOlder()) {
    const int xs[] = {x0 + 8, x0 + 8 + ARROW_HALF, x0 + 8 + ARROW_HALF};
    const int ys[] = {arrowMid, arrowMid - ARROW_HALF, arrowMid + ARROW_HALF};
    renderer.fillPolygon(xs, ys, 3, true);
    screen.frame().hit(
        fui::Rect{static_cast<int16_t>(x0), static_cast<int16_t>(y), ARROW_WIDTH, static_cast<int16_t>(infoHeight)},
        ACTION_PAGE, 1, fui::InputTouch);
  }
  if (pageBack[tab] > 0) {
    const int right = x0 + width - 8;
    const int xs[] = {right, right - ARROW_HALF, right - ARROW_HALF};
    const int ys[] = {arrowMid, arrowMid - ARROW_HALF, arrowMid + ARROW_HALF};
    renderer.fillPolygon(xs, ys, 3, true);
    screen.frame().hit(fui::Rect{static_cast<int16_t>(x0 + width - ARROW_WIDTH), static_cast<int16_t>(y), ARROW_WIDTH,
                                 static_cast<int16_t>(infoHeight)},
                       ACTION_PAGE, -1, fui::InputTouch);
  }
  y += infoHeight;

  // Window total and average, or why calendar data is missing.
  uint32_t total = 0;
  uint32_t peak = 0;
  for (int i = 0; i < barCount; ++i) {
    total += bars[i];
    peak = std::max(peak, bars[i]);
  }
  if (!clockKnown) {
    snprintf(text, sizeof(text), "%s", tr(STR_STATS_CLOCK_UNKNOWN));
  } else {
    char totalText[32];
    char averageText[32];
    statstext::duration(total, totalText, sizeof(totalText));
    statstext::duration(total / static_cast<uint32_t>(barCount), averageText, sizeof(averageText));
    snprintf(text, sizeof(text), tr(STR_STATS_WINDOW_SUMMARY), totalText, averageText);
  }
  const std::string summary = renderer.truncatedText(SMALL_FONT_ID, text, width);
  renderer.drawText(SMALL_FONT_ID, x0 + (width - renderer.getTextWidth(SMALL_FONT_ID, summary.c_str())) / 2, y,
                    summary.c_str(), true);
  y += smallHeight + 8;

  // Plot: gridlines at a round step, bars, axis labels below.
  uint32_t step = GRID_STEPS[sizeof(GRID_STEPS) / sizeof(GRID_STEPS[0]) - 1];
  for (const uint32_t candidate : GRID_STEPS) {
    if (peak <= candidate * MAX_GRIDLINES) {
      step = candidate;
      break;
    }
  }
  const uint32_t top = std::max<uint32_t>(step, (peak + step - 1) / step * step);
  char label[16];
  statstext::compactDuration(top, label, sizeof(label));
  const int axisWidth = renderer.getTextWidth(SMALL_FONT_ID, label) + 6;
  const int plotTop = y + smallHeight / 2 + smallHeight;  // room for value labels over the top bar
  const int labelBand = smallHeight + 6;
  const int plotBottom = bottom - labelBand;
  const int plotHeight = plotBottom - plotTop;
  const int slot = (width - axisWidth) / barCount;
  if (plotHeight < 20 || slot < 2) return;
  const int plotWidth = slot * barCount;
  const int plotX = x0 + width - plotWidth;

  for (uint32_t v = step; v <= top; v += step) {
    const int gy = plotBottom - static_cast<int>(static_cast<uint64_t>(v) * plotHeight / top);
    for (int gx = plotX; gx < plotX + plotWidth; gx += 4) renderer.drawPixel(gx, gy, true);
    statstext::compactDuration(v, label, sizeof(label));
    renderer.drawText(SMALL_FONT_ID, plotX - 4 - renderer.getTextWidth(SMALL_FONT_ID, label), gy - smallHeight / 2,
                      label, true);
  }

  const int barWidth = std::clamp(slot * 3 / 5, 2, 40);
  const bool valueLabels = barCount <= 12;
  int lastLabelEnd = INT32_MIN;
  for (int i = 0; i < barCount; ++i) {
    const int slotX = plotX + i * slot;
    if (i == focus && ringPos() > 0) {
      renderer.fillRectDither(slotX, plotTop, slot, plotBottom - plotTop, Color::LightGray);
    }
    int barHeight = static_cast<int>(static_cast<uint64_t>(bars[i]) * plotHeight / top);
    if (bars[i] > 0) barHeight = std::max(barHeight, 2);
    const int barX = slotX + (slot - barWidth) / 2;
    if (barHeight > 0) renderer.fillRect(barX, plotBottom - barHeight, barWidth, barHeight, true);
    if (i == focus) {
      // Marker under the focused bar.
      renderer.fillRect(barX, plotBottom + 2, barWidth, 2, true);
    }
    if (valueLabels && bars[i] > 0) {
      statstext::compactDuration(bars[i], label, sizeof(label));
      const int w = renderer.getTextWidth(SMALL_FONT_ID, label);
      if (w <= slot) {
        renderer.drawText(SMALL_FONT_ID, slotX + (slot - w) / 2, plotBottom - barHeight - smallHeight - 2, label, true);
      }
    }
    if (barAxisLabel(i, label, sizeof(label))) {
      const int w = renderer.getTextWidth(SMALL_FONT_ID, label);
      const int lx = std::max(slotX + (slot - w) / 2, x0);
      if (lx > lastLabelEnd + 4 && lx + w <= x0 + width) {
        renderer.drawText(SMALL_FONT_ID, lx, plotBottom + 5, label, true);
        lastLabelEnd = lx + w;
      }
    }
  }
  renderer.drawLine(plotX, plotBottom, plotX + plotWidth - 1, plotBottom, true);

  // One touch target over the plot: the tap position picks the bar.
  screen.frame().hit(fui::Rect{static_cast<int16_t>(plotX), static_cast<int16_t>(plotTop),
                               static_cast<int16_t>(plotWidth), static_cast<int16_t>(bottom - plotTop)},
                     ACTION_CHART, 0, fui::InputTouch | fui::InputDrag);
}
