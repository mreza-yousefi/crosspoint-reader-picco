#include "ReadingStatsHooksActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "StatsText.h"
#include "components/UITheme.h"
#include "stats/ReadingStats.h"

namespace fui = freeink::ui;

ReadingStatsHooksActivity::ReadingStatsHooksActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                     const uint16_t bookIndex)
    : UiTabListActivity("ReadingStatsHooks", renderer, mappedInput), bookIndex(bookIndex) {
  tabPillMaxPad = 8;
}

const char* ReadingStatsHooksActivity::headerTitle() const { return tr(STR_STATS_WHAT_HOOKED_ME); }

void ReadingStatsHooksActivity::onEnter() {
  READING_STATS.setPaused(true);
  UiTabListActivity::onEnter();
  for (int i = 0; i < ROW_COUNT; ++i) rows[i].actionValue = static_cast<int16_t>(i);
  rows[ROW_ONE_MORE].label = tr(STR_STATS_ONE_MORE_CHAPTER);
  rows[ROW_MOST_TIME].label = tr(STR_STATS_MOST_TIME);
  rows[ROW_FASTEST].label = tr(STR_STATS_FASTEST_CHAPTER);
  rows[ROW_SLOWEST].label = tr(STR_STATS_SLOWEST_CHAPTER);
  rows[ROW_MOST_STARTS].label = tr(STR_STATS_MOST_STARTS);
  rows[ROW_MOST_STOPS].label = tr(STR_STATS_MOST_STOPS);
  rows[ROW_LAST_STOP].label = tr(STR_STATS_LAST_STOP);
  rows[ROW_MOST_REREAD].label = tr(STR_STATS_MOST_REREAD);
  rows[ROW_MOST_BACK].label = tr(STR_STATS_MOST_TURNS_BACK);
  rows[ROW_MOST_PAUSES].label = tr(STR_STATS_MOST_PAUSES);
  rows[ROW_MOST_LOOKUPS].label = tr(STR_STATS_MOST_LOOKUPS);
  {
    RenderLock lock;
    GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
    loaded = stats::loadBookHooks(bookIndex, hooks);
    if (!loaded) LOG_ERR("STATS", "Could not load the chapters of book %u", static_cast<unsigned>(bookIndex));
    formatHighlights();
  }
  requestUpdate();
}

void ReadingStatsHooksActivity::chapterName(const int chapter, char* out, const size_t size) const {
  if (chapter < 0 || static_cast<size_t>(chapter) >= hooks.count) {
    snprintf(out, size, "-");
    return;
  }
  const char* title = hooks.titles.c_str() + hooks.titleAt[chapter];
  if (title[0] != '\0') {
    snprintf(out, size, "%s", title);
  } else {
    snprintf(out, size, tr(STR_STATS_CHAPTER_N), static_cast<unsigned>(hooks.chapters[chapter].spine) + 1);
  }
}

void ReadingStatsHooksActivity::setHighlight(const int row, const int chapter, const char* value) {
  rows[row].value = values[row];
  rowChapter[row] = chapter;
  if (chapter < 0) {
    snprintf(values[row], sizeof(values[row]), "-");
    rows[row].subtitle = nullptr;
    return;
  }
  snprintf(values[row], sizeof(values[row]), "%s", value);
  chapterName(chapter, subtitles[row], sizeof(subtitles[row]));
  rows[row].subtitle = subtitles[row];
}

void ReadingStatsHooksActivity::formatHighlights() {
  const stats::ChapterStats* c = hooks.chapters.get();
  const stats::HookHighlights h = stats::pickHighlights(c, hooks.count);
  const auto count = [](const unsigned n, char* out, const size_t size) { snprintf(out, size, "%u", n); };
  char text[32] = {};

  rows[ROW_ONE_MORE].value = values[ROW_ONE_MORE];
  rows[ROW_ONE_MORE].subtitle = nullptr;
  rowChapter[ROW_ONE_MORE] = -1;
  snprintf(values[ROW_ONE_MORE], sizeof(values[ROW_ONE_MORE]), tr(STR_STATS_N_OF_M_SESSIONS),
           static_cast<unsigned>(hooks.totals.oneMoreSessions), static_cast<unsigned>(hooks.totals.sessions));

  if (h.mostTime >= 0) statstext::duration(c[h.mostTime].activeMs / 1000, text, sizeof(text));
  setHighlight(ROW_MOST_TIME, h.mostTime, text);
  if (h.fastest >= 0) {
    snprintf(text, sizeof(text), tr(STR_STATS_PAGES_PER_HOUR),
             static_cast<unsigned>(stats::pagesPerHour(c[h.fastest].charsRead, c[h.fastest].learnedMs)));
  }
  setHighlight(ROW_FASTEST, h.fastest, text);
  if (h.slowest >= 0) {
    snprintf(text, sizeof(text), tr(STR_STATS_PAGES_PER_HOUR),
             static_cast<unsigned>(stats::pagesPerHour(c[h.slowest].charsRead, c[h.slowest].learnedMs)));
  }
  setHighlight(ROW_SLOWEST, h.slowest, text);
  if (h.mostStarts >= 0) count(c[h.mostStarts].starts, text, sizeof(text));
  setHighlight(ROW_MOST_STARTS, h.mostStarts, text);
  if (h.mostEnds >= 0) count(c[h.mostEnds].ends, text, sizeof(text));
  setHighlight(ROW_MOST_STOPS, h.mostEnds, text);

  // The chapter row holding the spine item the last session ended in.
  int lastStop = -1;
  for (size_t i = 0; hooks.totals.hasLastEnd && i < hooks.count && c[i].spine <= hooks.totals.lastEndSpine; ++i) {
    lastStop = static_cast<int>(i);
  }
  setHighlight(ROW_LAST_STOP, lastStop, "");

  if (h.mostReread >= 0) {
    const uint32_t pages = (c[h.mostReread].rereadChars + stats::PAPER_PAGE_CHARS / 2) / stats::PAPER_PAGE_CHARS;
    if (pages <= 1) {
      snprintf(text, sizeof(text), "%s", tr(STR_STATS_ONE_PAGE_AGAIN));
    } else {
      snprintf(text, sizeof(text), tr(STR_STATS_REREAD_PAGES), static_cast<unsigned>(pages));
    }
  }
  setHighlight(ROW_MOST_REREAD, h.mostReread, text);
  if (h.mostBack >= 0) count(c[h.mostBack].turnsBack, text, sizeof(text));
  setHighlight(ROW_MOST_BACK, h.mostBack, text);
  if (h.mostPauses >= 0) count(c[h.mostPauses].pauses, text, sizeof(text));
  setHighlight(ROW_MOST_PAUSES, h.mostPauses, text);
  if (h.mostLookups >= 0) count(c[h.mostLookups].lookups, text, sizeof(text));
  setHighlight(ROW_MOST_LOOKUPS, h.mostLookups, text);
}

int ReadingStatsHooksActivity::listCount() const {
  if (hooks.count == 0) return 0;
  return tab == TAB_HIGHLIGHTS ? ROW_COUNT : static_cast<int>(hooks.count);
}

const char* ReadingStatsHooksActivity::tabLabel(const int index) const {
  return index == TAB_CHAPTERS ? tr(STR_STATS_CHAPTERS) : tr(STR_STATS_HIGHLIGHTS);
}

void ReadingStatsHooksActivity::selectTab(const int index) {
  if (index < 0 || index >= TAB_COUNT) return;
  {
    RenderLock lock;
    tab = index;
  }
  activeNav().selected = 0;
  activeNav().top = 0;
  requestUpdate();
}

void ReadingStatsHooksActivity::onTabAction(const int index) {
  app.clearTapFlash();
  selectTab(index);
}

void ReadingStatsHooksActivity::stepTab(const int direction) {
  selectTab((tab + (direction > 0 ? 1 : TAB_COUNT - 1)) % TAB_COUNT);
}

void ReadingStatsHooksActivity::activateIndex(const int index) {
  if (tab != TAB_HIGHLIGHTS || index < 0 || index >= ROW_COUNT || rowChapter[index] < 0) return;
  app.clearTapFlash();
  const int chapter = rowChapter[index];
  {
    RenderLock lock;
    tab = TAB_CHAPTERS;
  }
  moveRingTo(chapter + 1);
  requestUpdate();
}

bool ReadingStatsHooksActivity::handleButtons() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return true;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    const int ring = ringPos();
    if (ring == 0) {
      stepTab(1);
    } else if (ring - 1 < listCount()) {
      activateIndex(ring - 1);
    }
    return true;
  }
  return false;
}

void ReadingStatsHooksActivity::drawFooter() {
  const int ring = ringPos();
  const char* confirm = "";
  if (ring == 0) {
    confirm = tr(STR_TOGGLE);
  } else if (tab == TAB_HIGHLIGHTS && ring - 1 < ROW_COUNT && rowChapter[ring - 1] >= 0) {
    confirm = tr(STR_OPEN);
  }
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirm, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void ReadingStatsHooksActivity::provideChapterRow(void* ctx, const uint16_t index, fui::ListItem& item) {
  auto* self = static_cast<ReadingStatsHooksActivity*>(ctx);
  if (index >= self->hooks.count) return;
  const stats::ChapterStats& c = self->hooks.chapters[index];
  self->chapterName(index, self->rowLabel, sizeof(self->rowLabel));
  int len = 0;
  const uint32_t speed = stats::pagesPerHour(c.charsRead, c.learnedMs);
  if (speed > 0) {
    len = snprintf(self->rowSubtitle, sizeof(self->rowSubtitle), tr(STR_STATS_PAGES_PER_HOUR),
                   static_cast<unsigned>(speed));
    if (len > 0 && static_cast<size_t>(len) < sizeof(self->rowSubtitle)) {
      len += snprintf(self->rowSubtitle + len, sizeof(self->rowSubtitle) - len, " · ");
    }
  }
  if (len >= 0 && static_cast<size_t>(len) < sizeof(self->rowSubtitle)) {
    snprintf(self->rowSubtitle + len, sizeof(self->rowSubtitle) - len, tr(STR_STATS_CHAPTER_DETAIL),
             static_cast<unsigned>(c.starts), static_cast<unsigned>(c.ends), static_cast<unsigned>(c.turnsBack),
             static_cast<unsigned>(c.pauses));
  }
  statstext::duration(c.activeMs / 1000, self->rowValue, sizeof(self->rowValue));
  item.label = self->rowLabel;
  item.subtitle = self->rowSubtitle;
  item.value = self->rowValue;
  item.actionValue = static_cast<int16_t>(index);
}

void ReadingStatsHooksActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  const int rightInset = renderer.getScreenWidth() - (safe.x + safe.width);
  const int bottomInset = renderer.getScreenHeight() - (safe.y + safe.height);
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), static_cast<int16_t>(rightInset),
                  static_cast<int16_t>(bottomInset + metrics.buttonHintsHeight), static_cast<int16_t>(safe.x)});
  buildTabBar(screen);
  if (hooks.count == 0) {
    screen.centeredText(tr(STR_STATS_NO_CHAPTERS));
    return;
  }
  fui::ListProps props;
  if (tab == TAB_HIGHLIGHTS) {
    props.items = rows;
    props.count = ROW_COUNT;
  } else {
    props.rowProvider = &ReadingStatsHooksActivity::provideChapterRow;
    props.rowProviderCtx = this;
    props.count = static_cast<uint16_t>(hooks.count);
  }
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncTabListViewport(screen, props);
  screen.list(props);
}
