#pragma once

#include <string>

#include "activities/UiListActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "backup/BackupConfig.h"

// Settings > Connectivity > Home Backup: server, login, automatic backup,
// last result, and "Back up now".
class BackupSettingsActivity final : public UiListActivity {
 public:
  BackupSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  static constexpr int MENU_ITEMS = 7;

  void onEnter() override;

 private:
  BackupConfig config;

  int listCount() const override { return MENU_ITEMS; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onRowAction(const freeink::ui::ActionEvent& event) override;
  bool handleCustomInput() override;
  const char* headerTitle() const override;

  void editText(StrId title, std::string BackupConfig::* field, size_t maxLen, InputType type);
  // Rows whose tap opens another screen.
  bool opensScreen(int index) const;

  // Tapped row drawn pressed by the next render, then opened. Written by the
  // loop task under the render lock; the render task reads it and sets
  // pressedPainted_.
  int pressedRow_ = -1;
  bool pressedPainted_ = false;

  std::string rowValues_[MENU_ITEMS];
  freeink::ui::ListItem rowItems_[MENU_ITEMS]{};
};
