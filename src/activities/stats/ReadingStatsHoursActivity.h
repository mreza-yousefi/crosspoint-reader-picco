#pragma once

#include <cstdint>

#include "activities/UiTabListActivity.h"
#include "stats/StatsLogic.h"

// When reading happens: a weekday x hour grid shaded by active time over the last 30 days,
// 12 months or all time. The buttons walk the weekday rows; the line above the grid gives the
// focused day's total and busiest hour.
class ReadingStatsHoursActivity final : public UiTabListActivity {
 public:
  ReadingStatsHoursActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;

 private:
  enum Tab : int { TAB_30_DAYS = 0, TAB_12_MONTHS, TAB_ALL, TAB_COUNT };
  static constexpr int WEEKDAYS = 7;

  int listCount() const override { return maxCell > 0 ? WEEKDAYS : 0; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int) override { requestUpdate(); }
  const char* headerTitle() const override;
  bool handleButtons() override;
  void drawFooter() override;
  int tabCount() const override { return TAB_COUNT; }
  int activeTab() const override { return tab; }
  const char* tabLabel(int index) const override;
  void onTabAction(int index) override;
  void stepTab(int direction) override;

  // Loop task; the caller holds the render lock once the screen is up.
  void loadGrid();
  void selectTab(int index);
  void drawGrid(UiScreen& screen, freeink::ui::Rect area);
  // Total and busiest hour of a weekday (-1 = all days).
  void dayInfo(int weekday, char* out, size_t size) const;

  int tab = TAB_30_DAYS;
  uint32_t grid[stats::GRID_CELLS] = {};
  uint32_t maxCell = 0;
};
