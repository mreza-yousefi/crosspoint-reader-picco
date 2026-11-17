#include "ReadingStatsPeriodActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "ReadingStatsDayActivity.h"
#include "StatsText.h"
#include "components/UITheme.h"
#include "stats/StatsQueries.h"

namespace fui = freeink::ui;

ReadingStatsPeriodActivity::ReadingStatsPeriodActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                       const int32_t firstDay, const int32_t lastDay, const char* title)
    : UiListActivity("ReadingStatsPeriod", renderer, mappedInput), firstDay(firstDay), lastDay(lastDay) {
  snprintf(this->title, sizeof(this->title), "%s", title);
}

void ReadingStatsPeriodActivity::onEnter() {
  stats::DayTotals totals;
  stats::loadDayTotals(totals);
  days.clear();
  for (const auto& day : totals.days) {
    if (day.day >= firstDay && day.day <= lastDay && day.activeSec > 0) days.push_back(day);
  }
  UiListActivity::onEnter();
}

void ReadingStatsPeriodActivity::provideRow(void* ctx, const uint16_t index, fui::ListItem& item) {
  auto* self = static_cast<ReadingStatsPeriodActivity*>(ctx);
  if (index >= self->days.size()) return;
  const stats::DayRecord& day = self->days[index];
  statstext::dayLong(day.day, false, self->rowLabel, sizeof(self->rowLabel));
  if (day.sessions == 1) {
    snprintf(self->rowSubtitle, sizeof(self->rowSubtitle), "%s", tr(STR_STATS_ONE_SESSION));
  } else {
    snprintf(self->rowSubtitle, sizeof(self->rowSubtitle), tr(STR_STATS_SESSIONS_COUNT), day.sessions);
  }
  statstext::duration(day.activeSec, self->rowValue, sizeof(self->rowValue));
  item.label = self->rowLabel;
  item.subtitle = self->rowSubtitle;
  item.value = self->rowValue;
  item.actionValue = static_cast<int16_t>(index);
}

void ReadingStatsPeriodActivity::activateIndex(const int index) {
  if (index < 0 || index >= static_cast<int>(days.size())) return;
  app.clearTapFlash();
  auto dayList = makeUniqueNoThrow<ReadingStatsDayActivity>(renderer, mappedInput, days[index].day);
  if (!dayList) {
    LOG_ERR("STATS", "OOM: day list");
    return;
  }
  startActivityForResult(std::move(dayList), [this](const ActivityResult&) { requestUpdate(); });
}

void ReadingStatsPeriodActivity::drawFooter() {
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), days.empty() ? "" : tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void ReadingStatsPeriodActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  if (days.empty()) {
    screen.centeredText(tr(STR_STATS_NO_READING));
    return;
  }
  fui::ListProps props;
  props.rowProvider = &ReadingStatsPeriodActivity::provideRow;
  props.rowProviderCtx = this;
  props.count = static_cast<uint16_t>(days.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}
