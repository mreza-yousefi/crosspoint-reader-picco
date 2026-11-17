#pragma once

#include <cstdint>

#include "activities/UiTabListActivity.h"
#include "stats/StatsQueries.h"

// What hooked me: one book's reading by chapter, from its raw log. Highlights: the chapters
// with the most of each (Confirm shows that chapter); Chapters: time, speed, starts, stops,
// turns back and pauses of each chapter read.
class ReadingStatsHooksActivity final : public UiTabListActivity {
 public:
  ReadingStatsHooksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, uint16_t bookIndex);
  void onEnter() override;

 private:
  enum Tab : int { TAB_HIGHLIGHTS = 0, TAB_CHAPTERS, TAB_COUNT };
  enum Row : int {
    ROW_ONE_MORE = 0,
    ROW_MOST_TIME,
    ROW_FASTEST,
    ROW_SLOWEST,
    ROW_MOST_STARTS,
    ROW_MOST_STOPS,
    ROW_LAST_STOP,
    ROW_MOST_REREAD,
    ROW_MOST_BACK,
    ROW_MOST_PAUSES,
    ROW_MOST_LOOKUPS,
    ROW_COUNT
  };

  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  bool handleButtons() override;
  void drawFooter() override;
  int tabCount() const override { return TAB_COUNT; }
  int activeTab() const override { return tab; }
  const char* tabLabel(int index) const override;
  void onTabAction(int index) override;
  void stepTab(int direction) override;

  void selectTab(int index);
  void formatHighlights();
  // Value and chapter subtitle of a highlight row; "-" for chapter < 0.
  void setHighlight(int row, int chapter, const char* value);
  void chapterName(int chapter, char* out, size_t size) const;
  static void provideChapterRow(void* ctx, uint16_t index, freeink::ui::ListItem& item);

  const uint16_t bookIndex;
  int tab = TAB_HIGHLIGHTS;
  bool loaded = false;
  stats::BookHooks hooks;
  int rowChapter[ROW_COUNT] = {};
  char values[ROW_COUNT][32] = {};
  char subtitles[ROW_COUNT][64] = {};
  freeink::ui::ListItem rows[ROW_COUNT]{};
  // Chapter row text for provideChapterRow; valid until its next call.
  char rowLabel[64] = {};
  char rowSubtitle[96] = {};
  char rowValue[24] = {};
};
