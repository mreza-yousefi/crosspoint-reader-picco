#include "ReadingStatsBooksActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "ReadingStatsBookActivity.h"
#include "ReadingStatsCompareActivity.h"
#include "StatsText.h"
#include "components/UITheme.h"
#include "stats/ReadingStats.h"

namespace fui = freeink::ui;

ReadingStatsBooksActivity::ReadingStatsBooksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                     const Mode mode, const int compareWith)
    : UiListActivity("ReadingStatsBooks", renderer, mappedInput), mode(mode), compareWith(compareWith) {}

void ReadingStatsBooksActivity::onEnter() {
  load();
  UiListActivity::onEnter();
}

void ReadingStatsBooksActivity::load() {
  stats::loadBookList(mode == Mode::Hidden, books);
  if (mode == Mode::Pick) {
    for (auto it = books.begin(); it != books.end(); ++it) {
      if (it->index == compareWith) {
        books.erase(it);
        break;
      }
    }
  }
  std::vector<uint16_t> indices;
  indices.reserve(books.size());
  for (const auto& book : books) indices.push_back(book.index);
  READING_STATS.bookTitles(indices, titles);
  for (auto& title : titles) {
    if (title.empty()) title = tr(STR_STATS_UNKNOWN_BOOK);
  }
}

const char* ReadingStatsBooksActivity::headerTitle() const {
  switch (mode) {
    case Mode::Hidden:
      return tr(STR_STATS_HIDDEN_BOOKS);
    case Mode::Pick:
      return tr(STR_STATS_COMPARE);
    case Mode::All:
      break;
  }
  return tr(STR_STATS_BOOKS);
}

void ReadingStatsBooksActivity::provideRow(void* ctx, const uint16_t index, fui::ListItem& item) {
  auto* self = static_cast<ReadingStatsBooksActivity*>(ctx);
  if (index >= self->books.size() || index >= self->titles.size()) return;
  const auto& book = self->books[index];
  item.label = self->titles[index].c_str();
  const char* style = statstext::styleName(book.style);
  if (book.lastUtc != 0) {
    char day[24];
    statstext::dayShort(book.lastDay, day, sizeof(day));
    snprintf(self->rowSubtitle, sizeof(self->rowSubtitle), "%s · %s", day, style);
  } else {
    snprintf(self->rowSubtitle, sizeof(self->rowSubtitle), "%s", style);
  }
  item.subtitle = self->rowSubtitle;
  if (book.counted) {
    statstext::duration(book.activeSec, self->rowValue, sizeof(self->rowValue));
    item.value = self->rowValue;
  }
  item.actionValue = static_cast<int16_t>(index);
}

void ReadingStatsBooksActivity::activateIndex(const int index) {
  if (index < 0 || index >= static_cast<int>(books.size())) return;
  app.clearTapFlash();
  const uint16_t book = books[index].index;
  if (mode == Mode::Pick) {
    auto compare =
        makeUniqueNoThrow<ReadingStatsCompareActivity>(renderer, mappedInput, static_cast<uint16_t>(compareWith), book);
    if (!compare) {
      LOG_ERR("STATS", "OOM: compare");
      return;
    }
    startActivityForResult(std::move(compare), [this](const ActivityResult&) { requestUpdate(); });
    return;
  }
  auto detail = makeUniqueNoThrow<ReadingStatsBookActivity>(renderer, mappedInput, book);
  if (!detail) {
    LOG_ERR("STATS", "OOM: book stats");
    return;
  }
  startActivityForResult(std::move(detail), [this](const ActivityResult&) {
    // The book may have been hidden or shown again.
    {
      RenderLock lock;
      load();
      if (nav.selected >= listCount()) nav.selected = listCount() > 0 ? listCount() - 1 : 0;
    }
    requestUpdate();
  });
}

void ReadingStatsBooksActivity::drawFooter() {
  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), books.empty() ? "" : tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void ReadingStatsBooksActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  if (books.empty()) {
    screen.centeredText(mode == Mode::Hidden ? tr(STR_STATS_NO_HIDDEN_BOOKS) : tr(STR_STATS_NO_BOOKS));
    return;
  }
  fui::ListProps props;
  props.rowProvider = &ReadingStatsBooksActivity::provideRow;
  props.rowProviderCtx = this;
  props.count = static_cast<uint16_t>(books.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}
