#pragma once

#include <cstdint>

#include "activities/UiTabListActivity.h"
#include "stats/StatsInsights.h"

// Reading speed (paper pages per hour) by month, part of the day and font size, from the
// measured reading of dated sessions.
class ReadingStatsSpeedActivity final : public UiTabListActivity {
 public:
  ReadingStatsSpeedActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;

 private:
  enum Tab : int { TAB_MONTHS = 0, TAB_DAY_PARTS, TAB_FONTS, TAB_COUNT };
  static constexpr int MAX_ROWS = stats::SPEED_MONTHS > stats::MAX_FONT_SIZES ? stats::SPEED_MONTHS
                                                                              : stats::MAX_FONT_SIZES;

  int listCount() const override { return rowCount; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int) override {}
  const char* headerTitle() const override;
  bool handleButtons() override;
  void drawFooter() override;
  int tabCount() const override { return TAB_COUNT; }
  int activeTab() const override { return tab; }
  const char* tabLabel(int index) const override;
  void onTabAction(int index) override;
  void stepTab(int direction) override;

  void selectTab(int index);
  // Rows of the active tab; the caller holds the render lock once the screen is up.
  void formatRows();
  void setRow(int row, const char* label, const stats::SpeedCell& cell);

  int tab = TAB_MONTHS;
  stats::SpeedTrends trends;
  int rowCount = 0;
  char labels[MAX_ROWS][24] = {};
  char values[MAX_ROWS][24] = {};
  char subtitles[MAX_ROWS][40] = {};
  freeink::ui::ListItem rows[MAX_ROWS]{};
};
