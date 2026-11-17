#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "activities/UiListActivity.h"

// The counted reading sessions of one local day: book, start and end time, active time,
// dictionary time. Confirm opens the session's book.
class ReadingStatsDayActivity final : public UiListActivity {
 public:
  ReadingStatsDayActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int32_t day);
  void onEnter() override;

 private:
  int listCount() const override { return static_cast<int>(items.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override { return title; }
  void drawFooter() override;

  int32_t day;
  char title[48] = {};
  // Built once in onEnter; items point into the strings.
  std::vector<std::string> labels;
  std::vector<std::string> subtitles;
  std::vector<std::string> values;
  std::vector<freeink::ui::ListItem> items;
  std::vector<uint16_t> books;  // book index per row
};
