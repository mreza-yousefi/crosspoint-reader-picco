#pragma once

#include <cstdint>

#include "activities/UiTabListActivity.h"
#include "stats/StatsLogic.h"
#include "stats/StatsQueries.h"

// Reading statistics: an Overview list and bar charts of active reading time per day,
// week, month and year. Chart tabs: the buttons walk the bars and page back in time at the
// oldest one; touch taps a bar, the arrows or swipes left/right to page. A day bar opens
// that day's sessions.
class ReadingStatsActivity final : public UiTabListActivity {
 public:
  // fromReader: pushed over a book (Back returns to it); otherwise opened from Home.
  ReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool fromReader);
  void onEnter() override;

 private:
  enum Tab : int { TAB_OVERVIEW = 0, TAB_DAYS, TAB_WEEKS, TAB_MONTHS, TAB_YEARS, TAB_COUNT };
  enum Row : int {
    ROW_TODAY = 0,
    ROW_WEEK,
    ROW_MONTH,
    ROW_YEAR,
    ROW_TOTAL,
    ROW_STREAK,
    ROW_LONGEST,
    ROW_FINISHED,
    ROW_IGNORE_SHORT,
    ROW_COUNT
  };
  static constexpr int MAX_BARS = 30;
  static constexpr int MAX_YEAR_BARS = 10;
  static constexpr freeink::ui::ActionId ACTION_CHART = ACTION_TAB_USER;
  static constexpr freeink::ui::ActionId ACTION_PAGE = ACTION_TAB_USER + 1;

  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  bool handleCustomInput() override;
  bool handleButtons() override;
  void navigateButtons() override;
  void drawFooter() override;
  int tabCount() const override { return TAB_COUNT; }
  int activeTab() const override { return tab; }
  const char* tabLabel(int index) const override;
  void onTabAction(int index) override;
  void stepTab(int direction) override;

  static void chartTrampoline(const freeink::ui::ActionEvent& event, void* user);
  static void pageTrampoline(const freeink::ui::ActionEvent& event, void* user);

  // Loop task; the caller holds the render lock when a render may be running.
  void loadData();
  void computeOverview();
  void computeBars();
  void selectTab(int index);
  bool chartTab() const { return tab != TAB_OVERVIEW; }
  stats::Range range() const;
  bool canPageOlder() const;
  void pageBy(int direction);
  void chartStep(int direction);
  // Bar the info line describes: the selected one, else the newest.
  int focusedBar() const;
  void openDay(int bar);
  void toggleIgnoreShortOpens();

  void drawChart(UiScreen& screen, freeink::ui::Rect area);
  void barTitle(int bar, char* out, size_t size) const;
  // Axis label under a bar, or false for none.
  bool barAxisLabel(int bar, char* out, size_t size) const;

  const bool fromReader;
  int tab = TAB_OVERVIEW;
  stats::DayTotals data;
  bool clockKnown = false;
  int32_t todayDay = 0;
  int finishedThisYear = 0;
  stats::Streaks streaks;
  char rowValues[ROW_COUNT][32] = {};
  freeink::ui::ListItem rows[ROW_COUNT]{};
  int pageBack[TAB_COUNT] = {};  // windows back from the current one, per chart tab
  uint32_t bars[MAX_BARS] = {};
  int barCount = 0;
  int32_t firstBucket = 0;
};
