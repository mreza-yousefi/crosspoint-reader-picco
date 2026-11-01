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

  static constexpr int MENU_ITEMS = 6;

  void onEnter() override;

 private:
  BackupConfig config;

  int listCount() const override { return MENU_ITEMS; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  void editText(StrId title, std::string BackupConfig::* field, size_t maxLen, InputType type);

  std::string rowValues_[MENU_ITEMS];
  freeink::ui::ListItem rowItems_[MENU_ITEMS]{};
};
