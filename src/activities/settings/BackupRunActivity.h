#pragma once

#include <string>

#include "activities/Activity.h"
#include "backup/BackupConfig.h"

// "Back up now": joins WiFi, pushes changed files to the home server, and shows
// progress. The upload blocks the loop; the render task keeps the screen live.
class BackupRunActivity final : public Activity {
 public:
  BackupRunActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("BackupRun", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return state == RUNNING; }

 private:
  enum State { WIFI_SELECTION, RUNNING, DONE, FAILED };

  State state = WIFI_SELECTION;
  BackupConfig config;
  std::string detail;
  unsigned long lastPaintMs = 0;
  size_t fetched = 0;

  void onWifiSelectionComplete(bool success);
};
