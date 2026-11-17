#pragma once

#include <cstdint>

#include "activities/UiListActivity.h"
#include "stats/StatsFormat.h"

// Two books side by side: each row shows "first | second".
class ReadingStatsCompareActivity final : public UiListActivity {
 public:
  ReadingStatsCompareActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, uint16_t first, uint16_t second);
  void onEnter() override;

 private:
  enum Row : int {
    ROW_FIRST = 0,
    ROW_SECOND,
    ROW_TOTAL,
    ROW_SPEED,
    ROW_SESSIONS,
    ROW_SITTINGS,
    ROW_STYLE,
    ROW_DAYS,
    ROW_COUNT
  };

  int listCount() const override { return ROW_COUNT; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int) override {}
  const char* headerTitle() const override;
  void drawFooter() override;

  const uint16_t books[2];
  char titles[2][sizeof(stats::BookRecord::title)] = {};
  char rowValues[ROW_COUNT][56] = {};
  freeink::ui::ListItem rows[ROW_COUNT]{};
};
