#pragma once

#include <cstdint>
#include <vector>

#include "activities/UiListActivity.h"
#include "stats/StatsFormat.h"

// The reading days of a week or month (a bar of the Weeks or Months chart); Confirm opens a
// day's sessions.
class ReadingStatsPeriodActivity final : public UiListActivity {
 public:
  ReadingStatsPeriodActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int32_t firstDay, int32_t lastDay,
                             const char* title);
  void onEnter() override;

 private:
  int listCount() const override { return static_cast<int>(days.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override { return title; }
  void drawFooter() override;
  static void provideRow(void* ctx, uint16_t index, freeink::ui::ListItem& item);

  const int32_t firstDay;
  const int32_t lastDay;
  char title[48] = {};
  std::vector<stats::DayRecord> days;  // with reading, in day order
  // Row text for provideRow; valid until its next call.
  char rowLabel[32] = {};
  char rowSubtitle[24] = {};
  char rowValue[24] = {};
};
