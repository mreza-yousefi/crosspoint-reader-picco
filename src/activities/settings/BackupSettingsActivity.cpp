#include "BackupSettingsActivity.h"

#include <I18n.h>

#include <cstdio>
#include <memory>

#include "BackupRunActivity.h"
#include "MappedInputManager.h"
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

constexpr StrId menuNames[BackupSettingsActivity::MENU_ITEMS] = {
    StrId::STR_SERVER_URL, StrId::STR_USERNAME,    StrId::STR_PASSWORD,
    StrId::STR_AUTO_BACKUP, StrId::STR_LAST_BACKUP, StrId::STR_BACKUP_NOW};
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

void BackupSettingsActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (index == IDX_SERVER_URL) {
    editText(StrId::STR_SERVER_URL, &BackupConfig::serverUrl, 128, InputType::Url);
  } else if (index == IDX_USERNAME) {
    editText(StrId::STR_USERNAME, &BackupConfig::username, 64, InputType::Text);
  } else if (index == IDX_PASSWORD) {
    editText(StrId::STR_PASSWORD, &BackupConfig::password, 128, InputType::Password);
  } else if (index == IDX_AUTO) {
    config.autoBackup = !config.autoBackup;
    config.save();
    requestUpdate();
  } else if (index == IDX_RUN) {
    if (!config.isConfigured()) return;
    startActivityForResult(std::make_unique<BackupRunActivity>(renderer, mappedInput),
                           [this](const ActivityResult&) {
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

  const int64_t age = backup::secondsSinceLastBackup(config);
  char last[64];
  if (age < 0) {
    snprintf(last, sizeof(last), "%s", config.lastSuccessEpoch > 0 ? "?" : tr(STR_NEVER));
  } else if (age < 2 * 3600) {
    snprintf(last, sizeof(last), "%d min", static_cast<int>(age / 60));
  } else if (age < 2 * 86400) {
    snprintf(last, sizeof(last), "%d h", static_cast<int>(age / 3600));
  } else {
    snprintf(last, sizeof(last), "%d d", static_cast<int>(age / 86400));
  }
  rowValues_[IDX_LAST] = last;
  if (config.lastResult != static_cast<int>(backup::Result::Ok)) {
    rowValues_[IDX_LAST] += std::string(" (") + backup::resultLabel(static_cast<backup::Result>(config.lastResult)) + ")";
  }
  rowValues_[IDX_RUN] = config.isConfigured() ? "" : std::string("[") + tr(STR_SET_CREDENTIALS_FIRST) + "]";

  for (int i = 0; i < MENU_ITEMS; i++) {
    rowItems_[i].value = rowValues_[i].empty() ? nullptr : rowValues_[i].c_str();
  }
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
