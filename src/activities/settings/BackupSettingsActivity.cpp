#include "BackupSettingsActivity.h"

#include <I18n.h>
#include <TrustedTime.h>

#include <cstdio>
#include <ctime>
#include <memory>

#include "BackupRunActivity.h"
#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "activities/RenderLock.h"
#include "backup/BackupEngine.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
constexpr int IDX_SERVER_URL = 0;
constexpr int IDX_USERNAME = 1;
constexpr int IDX_PASSWORD = 2;
constexpr int IDX_AUTO = 3;
constexpr int IDX_LAST = 4;
constexpr int IDX_RUN = 5;
constexpr int IDX_BROWSE = 6;

constexpr StrId menuNames[BackupSettingsActivity::MENU_ITEMS] = {
    StrId::STR_SERVER_URL,  StrId::STR_USERNAME,   StrId::STR_PASSWORD,       StrId::STR_AUTO_BACKUP,
    StrId::STR_LAST_BACKUP, StrId::STR_BACKUP_NOW, StrId::STR_BACKUP_WEB_VIEW};
}  // namespace

BackupSettingsActivity::BackupSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("BackupSettings", renderer, mappedInput) {
  for (int i = 0; i < MENU_ITEMS; i++) {
    rowItems_[i].label = I18N.get(menuNames[i]);
    rowItems_[i].actionValue = static_cast<int16_t>(i);
  }
}

void BackupSettingsActivity::onEnter() {
  config.load();  // also after returning from a run, for the new result
  UiListActivity::onEnter();
}

const char* BackupSettingsActivity::headerTitle() const { return tr(STR_BACKUP); }

void BackupSettingsActivity::editText(const StrId title, std::string BackupConfig::* field, const size_t maxLen,
                                      const InputType type) {
  startActivityForResult(
      std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, I18N.get(title), config.*field, maxLen, type),
      [this, field](const ActivityResult& result) {
        if (result.isCancelled) return;
        config.*field = std::get<KeyboardResult>(result.data).text;
        config.save();
      });
}

bool BackupSettingsActivity::opensScreen(const int index) const {
  return index == IDX_SERVER_URL || index == IDX_USERNAME || index == IDX_PASSWORD || index == IDX_BROWSE ||
         (index == IDX_RUN && config.isConfigured());
}

// A tap on a row that opens another screen shows its pressed state first, so
// it gets feedback before the slower screen change: the render the tap queues
// anyway draws the row pressed (no extra refresh), and the screen opens once
// that frame is on the panel. Buttons already show the selection.
void BackupSettingsActivity::onRowAction(const fui::ActionEvent& event) {
  if (event.longPress || !opensScreen(event.value)) {
    UiListActivity::onRowAction(event);
    return;
  }
  activeNav().selected = event.value;
  app.clearTapFlash();
  {
    RenderLock lock;
    pressedRow_ = event.value;
    pressedPainted_ = false;
  }
  requestUpdate();
}

bool BackupSettingsActivity::handleCustomInput() {
  if (pressedRow_ < 0) return false;  // only this (loop) task writes it
  int index;
  {
    // Blocks while the pressed frame is still being drawn and refreshed.
    RenderLock lock;
    if (!pressedPainted_) return true;  // not drawn yet: wait for the render
    index = pressedRow_;
    pressedRow_ = -1;
  }
  activateIndex(index);
  return true;
}

void BackupSettingsActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (index == IDX_SERVER_URL) {
    editText(StrId::STR_SERVER_URL, &BackupConfig::serverUrl, 128, InputType::Url);
  } else if (index == IDX_USERNAME) {
    editText(StrId::STR_USERNAME, &BackupConfig::username, 64, InputType::Text);
  } else if (index == IDX_PASSWORD) {
    editText(StrId::STR_PASSWORD, &BackupConfig::password, 128, InputType::Password);
  } else if (index == IDX_BROWSE) {
    editText(StrId::STR_BACKUP_WEB_VIEW, &BackupConfig::browseUrl, 128, InputType::Url);
  } else if (index == IDX_AUTO) {
    config.autoBackup = !config.autoBackup;
    config.save();
    requestUpdate();
  } else if (index == IDX_RUN) {
    if (!config.isConfigured()) return;
    startActivityForResult(std::make_unique<BackupRunActivity>(renderer, mappedInput), [this](const ActivityResult&) {
      config.load();
      requestUpdate();
    });
  }
}

void BackupSettingsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  rowValues_[IDX_SERVER_URL] = config.serverUrl.empty() ? tr(STR_NOT_SET) : config.serverUrl;
  rowValues_[IDX_USERNAME] = config.username.empty() ? tr(STR_NOT_SET) : config.username;
  rowValues_[IDX_PASSWORD] = config.password.empty() ? tr(STR_NOT_SET) : "******";
  rowValues_[IDX_AUTO].clear();
  rowValues_[IDX_BROWSE] = config.browseUrl.empty() ? tr(STR_NOT_SET) : config.browseUrl;

  // Local date and time of the last good backup, e.g. "Today 14:32" or "28 Oct 21:05".
  char last[64];
  if (config.lastSuccessEpoch <= 0) {
    snprintf(last, sizeof(last), "%s", tr(STR_NEVER));
  } else {
    const time_t when = static_cast<time_t>(config.lastSuccessEpoch);
    struct tm whenTm{};
    localtime_r(&when, &whenTm);
    char clock[16];
    strftime(clock, sizeof(clock), SETTINGS.clockFormat == 1 ? "%I:%M %p" : "%H:%M", &whenTm);
    const time_t now = static_cast<time_t>(trustedtime::trustedNow());
    struct tm nowTm{};
    if (now > 0) localtime_r(&now, &nowTm);
    const bool sameYear = now > 0 && nowTm.tm_year == whenTm.tm_year;
    if (sameYear && nowTm.tm_yday == whenTm.tm_yday) {
      snprintf(last, sizeof(last), "%s %s", tr(STR_TODAY), clock);
    } else if (sameYear && nowTm.tm_yday == whenTm.tm_yday + 1) {
      snprintf(last, sizeof(last), "%s %s", tr(STR_YESTERDAY), clock);
    } else {
      char date[16];
      strftime(date, sizeof(date), sameYear ? "%d %b" : "%d %b %Y", &whenTm);
      snprintf(last, sizeof(last), "%s %s", date, clock);
    }
  }
  rowValues_[IDX_LAST] = last;
  if (config.lastResult != static_cast<int>(backup::Result::Ok)) {
    rowValues_[IDX_LAST] +=
        std::string(" (") + backup::resultLabel(static_cast<backup::Result>(config.lastResult)) + ")";
  }
  rowValues_[IDX_RUN] = config.isConfigured() ? "" : std::string("[") + tr(STR_SET_CREDENTIALS_FIRST) + "]";

  for (int i = 0; i < MENU_ITEMS; i++) {
    rowItems_[i].value = rowValues_[i].empty() ? nullptr : rowValues_[i].c_str();
    rowItems_[i].state = i == pressedRow_ ? fui::StateActive : fui::StateNormal;
  }
  if (pressedRow_ >= 0) pressedPainted_ = true;
  GUI.setCheckboxRow(rowItems_[IDX_AUTO], config.autoBackup);

  fui::ListProps props;
  props.items = rowItems_;
  props.count = static_cast<uint16_t>(MENU_ITEMS);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}
