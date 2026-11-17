#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "activities/UiListActivity.h"
#include "stats/StatsQueries.h"

// Books in the reading statistics, last read first: time, last read day and reading style.
// Confirm opens the book's page; in the hidden list that is where a book is shown again.
// Pick mode chooses the second book of a comparison.
class ReadingStatsBooksActivity final : public UiListActivity {
 public:
  enum class Mode : uint8_t { All, Hidden, Pick };
  // compareWith: the first book, in Pick mode (left out of the list).
  ReadingStatsBooksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, Mode mode, int compareWith = -1);
  void onEnter() override;

 private:
  int listCount() const override { return static_cast<int>(books.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;
  void drawFooter() override;

  // Loop task; the caller holds the render lock once the screen is up.
  void load();
  static void provideRow(void* ctx, uint16_t index, freeink::ui::ListItem& item);

  const Mode mode;
  const int compareWith;
  std::vector<stats::BookListEntry> books;
  std::vector<std::string> titles;  // by row
  // Row text for provideRow; valid until its next call.
  char rowSubtitle[64] = {};
  char rowValue[32] = {};
};
