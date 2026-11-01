#include "BackupRunActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <WiFi.h>

#include <cstdio>
#include <string>

#include "MappedInputManager.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "backup/BackupEngine.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "network/WifiPowerSaveGuard.h"

namespace {
// E-ink refreshes take ~340 ms; repaint progress at most this often.
constexpr unsigned long PROGRESS_PAINT_MS = 2000;
}  // namespace

void BackupRunActivity::onEnter() {
  Activity::onEnter();
  config.load();
  if (WiFi.status() == WL_CONNECTED) {
    onWifiSelectionComplete(true);
    return;
  }
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) { onWifiSelectionComplete(!result.isCancelled); });
}

void BackupRunActivity::onWifiSelectionComplete(const bool success) {
  if (!success) {
    {
      RenderLock lock(*this);
      state = FAILED;
      detail = tr(STR_WIFI_CONN_FAILED);
    }
    requestUpdate();
    return;
  }
  {
    RenderLock lock(*this);
    state = RUNNING;
    detail.clear();
  }
  // The run below blocks this loop pass, so deferred updates would only paint
  // after it; ask the render task directly.
  requestUpdate(true);

  WifiPowerSaveGuard psGuard;
  const auto result = backup::runAndRecord(config, 0, [this](const backup::Progress& p) {
    fetched = p.filesFetched;
    const unsigned long now = millis();
    if (now - lastPaintMs >= PROGRESS_PAINT_MS || p.filesDone == p.filesPending) {
      lastPaintMs = now;
      char text[80];
      const int n = snprintf(text, sizeof(text), "%u / %u  (%.1f MB)", static_cast<unsigned>(p.filesDone),
                             static_cast<unsigned>(p.filesPending),
                             static_cast<double>(p.bytesDone) / (1024.0 * 1024.0));
      if (p.filesFetched > 0 && n > 0 && static_cast<size_t>(n) < sizeof(text)) {
        snprintf(text + n, sizeof(text) - n, "  +%u new", static_cast<unsigned>(p.filesFetched));
      }
      {
        RenderLock lock(*this);
        detail = text;
      }
      requestUpdate(true);
    }
    return true;
  });

  {
    RenderLock lock(*this);
    if (result == backup::Result::Ok) {
      state = DONE;
      if (fetched > 0) detail = "+" + std::to_string(fetched) + " new";
    } else {
      state = FAILED;
      detail = backup::resultLabel(result);
    }
  }
  requestUpdate();
}

void BackupRunActivity::onExit() {
  Activity::onExit();
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void BackupRunActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_BACKUP));

  const auto height = renderer.getLineHeight(UI_10_FONT_ID);
  const auto top = (pageHeight - height) / 2;
  if (state == RUNNING) {
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_BACKING_UP), true, EpdFontFamily::BOLD);
  } else if (state == DONE) {
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_BACKUP_COMPLETE), true, EpdFontFamily::BOLD);
  } else if (state == FAILED) {
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_BACKUP_FAILED), true, EpdFontFamily::BOLD);
  }
  if (!detail.empty()) renderer.drawCenteredText(UI_10_FONT_ID, top + height + 10, detail.c_str());

  const auto labels = mappedInput.mapLabels(state == RUNNING ? "" : tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void BackupRunActivity::loop() {
  if (state != DONE && state != FAILED) return;
  int x = 0;
  int y = 0;
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
      mappedInput.wasPressed(MappedInputManager::Button::Confirm) || mappedInput.wasScreenTapped(x, y)) {
    finish();
  }
}
