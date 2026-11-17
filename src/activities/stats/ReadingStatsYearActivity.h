#pragma once

#include <cstdint>

#include "activities/UiTabListActivity.h"
#include "stats/StatsInsights.h"

// Year in review: one tab per year with reading, newest first. Confirm on a top book opens
// its page.
class ReadingStatsYearActivity final : public UiTabListActivity {
 public:
  ReadingStatsYearActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;

 private:
  static constexpr int MAX_YEARS = 6;
  static constexpr int MAX_ROWS = 16;

  int listCount() const override { return rowCount; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  bool handleButtons() override;
  void drawFooter() override;
  int tabCount() const override { return yearCount > 0 ? yearCount : 1; }
  int activeTab() const override { return tab; }
  const char* tabLabel(int index) const override;
  void onTabAction(int index) override;
  void stepTab(int direction) override;

  void selectTab(int index);
  // Loads and formats the active year; the caller holds the render lock once the screen is up.
  void loadYear();
  // Appends a row with the label; its value is values[row], its subtitle unset.
  int addRow(const char* label);

  int tab = 0;
  int years[MAX_YEARS] = {};
  int yearCount = 0;
  char yearLabels[MAX_YEARS][8] = {};
  stats::YearReview review;
  int rowCount = 0;
  uint16_t rowBook[MAX_ROWS] = {};
  char topLabels[stats::YEAR_TOP_BOOKS][96] = {};
  char values[MAX_ROWS][32] = {};
  char subtitles[MAX_ROWS][80] = {};
  freeink::ui::ListItem rows[MAX_ROWS]{};
};
