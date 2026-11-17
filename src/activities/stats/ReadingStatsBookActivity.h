#pragma once

#include <cstdint>

#include "activities/UiTabListActivity.h"
#include "stats/StatsQueries.h"

// One book's reading statistics. Summary: totals, speed, style, time to finish, hide from the
// stats, compare; Progress: position over the reading days; Sessions: the newest sessions.
class ReadingStatsBookActivity final : public UiTabListActivity {
 public:
  ReadingStatsBookActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, uint16_t bookIndex);
  void onEnter() override;
  // Pushes a book's page over parent (Home, Library, the reader, the day list); nothing for
  // bookIndex < 0. False when it was not opened.
  static bool push(Activity& parent, GfxRenderer& renderer, MappedInputManager& mappedInput, int bookIndex);

 private:
  enum Tab : int { TAB_SUMMARY = 0, TAB_PROGRESS, TAB_SESSIONS, TAB_COUNT };
  enum Row : int {
    ROW_TOTAL = 0,
    ROW_SESSIONS,
    ROW_SITTINGS,
    ROW_AVERAGE,
    ROW_FIRST,
    ROW_LAST,
    ROW_STYLE,
    ROW_SPEED,
    ROW_FINISH,
    ROW_POSITION,
    ROW_LOOKUPS,
    ROW_DICTIONARY,
    ROW_HIDDEN,
    ROW_COMPARE,
    ROW_COUNT
  };
  static constexpr int MAX_COLUMNS = 60;

  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override { return title; }
  bool handleButtons() override;
  void drawFooter() override;
  int tabCount() const override { return TAB_COUNT; }
  int activeTab() const override { return tab; }
  const char* tabLabel(int index) const override;
  void onTabAction(int index) override;
  void stepTab(int direction) override;

  // Loop task; the caller holds the render lock once the screen is up.
  void load();
  void formatSummary();
  void selectTab(int index);
  void toggleHidden();
  void openCompare();
  void drawProgress(UiScreen& screen, freeink::ui::Rect area);
  static void provideSessionRow(void* ctx, uint16_t index, freeink::ui::ListItem& item);

  const uint16_t bookIndex;
  int tab = TAB_SUMMARY;
  char title[sizeof(stats::BookRecord::title)] = {};
  stats::BookDetail detail;
  char rowValues[ROW_COUNT][32] = {};
  freeink::ui::ListItem rows[ROW_COUNT]{};
  // Session row text for provideSessionRow; valid until its next call.
  char rowLabel[48] = {};
  char rowSubtitle[64] = {};
  char rowValue[24] = {};
};
