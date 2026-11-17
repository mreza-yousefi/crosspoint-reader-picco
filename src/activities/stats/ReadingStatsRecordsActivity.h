#pragma once

#include <cstdint>

#include "activities/UiListActivity.h"
#include "stats/StatsInsights.h"

// Personal records over all dated reading, then lookups per hour and late nights. Confirm on a
// record with a book opens that book's page.
class ReadingStatsRecordsActivity final : public UiListActivity {
 public:
  ReadingStatsRecordsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;

 private:
  enum Row : int {
    ROW_LONGEST_SESSION = 0,
    ROW_LONGEST_SITTING,
    ROW_MOST_DAY,
    ROW_MOST_WEEK,
    ROW_FASTEST,
    ROW_STREAK,
    ROW_MOST_LOOKUPS,
    ROW_LATEST,
    ROW_EARLIEST,
    ROW_LOOKUPS_PER_HOUR,
    ROW_LATE,
    ROW_COUNT
  };

  int listCount() const override { return ROW_COUNT; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void drawFooter() override;

  void format();
  // Value and "title · day" subtitle of a record with a book; "-" without one.
  void setBookRecord(int row, const stats::RecordRef& record, const char* value, const char* title);

  stats::Records records;
  uint16_t rowBook[ROW_COUNT] = {};
  char values[ROW_COUNT][32] = {};
  char subtitles[ROW_COUNT][96] = {};
  freeink::ui::ListItem rows[ROW_COUNT]{};
};
