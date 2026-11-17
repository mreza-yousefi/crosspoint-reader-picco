#include "DictionaryWordSelectActivity.h"

#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <Memory.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>

#include "CrossPointSettings.h"
#include "DictionaryDefinitionActivity.h"
#include "LookupButton.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "stats/ReadingStats.h"
#include "util/SlideSwitch.h"

namespace {

constexpr unsigned long POPUP_DURATION_MS = 1500;

void indexBuildYield(void*) { vTaskDelay(1); }

}  // namespace

void DictionaryWordSelectActivity::onEnter() {
  Activity::onEnter();
  // Looking a word up is part of reading, also when started from the menu.
  READING_STATS.setPaused(false);
  fontId = SETTINGS.getReaderFontId();
  // No busy popup by default: the pressed Look Up button already acknowledged
  // the tap, and each extra e-ink refresh here costs ~0.5 s (see performLookup).
  lookupPending = true;
  popup = Popup::None;
}

void DictionaryWordSelectActivity::performLookup() {
  dropLookupRecord();
  if (!dictOpenAttempted) {
    dictOpenAttempted = true;
    dictOpenOk = dict.open(SETTINGS.dictionaryName);
    // needsIndex() opens and validates the .qidx sidecar, so ask it once per
    // open rather than once per word: the answer only changes when we build
    // the sidecar ourselves, which is handled below.
    dictNeedsIndex = dictOpenOk && dict.needsIndex();
  }
  // One paint: the clean page the definition sheet will sit on. Only a first-use
  // index build (seconds) gets a busy popup, cleared again before the sheet.
  const bool showedBusy = dictNeedsIndex;
  {
    RenderLock lock;
    if (showedBusy) {
      popupMsg = StrId::STR_DICT_INDEXING;
      popup = Popup::Busy;
    }
    paintOnly = !showedBusy;  // a busy popup must be shown; a clean page need not be
  }
  requestUpdateAndWait();
  {
    RenderLock lock;
    paintOnly = false;
  }

  bool ok = dictOpenOk;
  Dictionary::IndexResult indexResult = Dictionary::IndexResult::Ok;
  if (ok && dictNeedsIndex) {
    ok = dict.buildIndex(&indexBuildYield, nullptr, &indexResult);
    dictNeedsIndex = !ok;  // a successful build leaves the sidecar fresh; a failed one retries
  }

  std::string definition;
  std::string headword;
  Dictionary::LookupResult result = Dictionary::LookupResult::NotFound;
  const bool found = ok && dict.lookup(lookupText.c_str(), definition, headword, &result);

  if (found) {
    if (showedBusy) {
      // The definition sheet keeps the frame above it: clear the popup first.
      {
        RenderLock lock;
        popup = Popup::None;
      }
      requestUpdateAndWait();
    }
    {
      RenderLock lock;
      page.reset();
    }
    // Kept in its saved form; written only once the definition has stayed on screen.
    recordVocabulary = SETTINGS.vocabularyHistoryEnabled &&
                       VocabularyStore::prepareDefinition(definition, dict.definitionsAreHtml(), recordDefinition);
    if (recordVocabulary) {
      recordWord = lookupText;
      recordHeadword = headword;
    } else if (SETTINGS.vocabularyHistoryEnabled) {
      LOG_ERR("VOCAB", "Could not prepare successful dictionary lookup");
    }
    auto activity = makeUniqueNoThrow<DictionaryDefinitionActivity>(renderer, mappedInput, std::move(headword),
                                                                    std::move(definition), dict.definitionsAreHtml());
    if (!activity || !activity->enableLookup(dict, std::move(lookupText))) {
      LOG_ERR("DICT", "OOM: definition activity");
      dropLookupRecord();
      popup = Popup::Error;
      popupMsg = StrId::STR_DICT_LOW_MEMORY;
      popupTime = millis();
      requestUpdate();
      return;
    }
    activity->onShownLongEnough([this] { recordLookup(); });
    // The sheet's one refresh also clears the pressed button's ghost (~+0.08 s).
    renderer.promoteNextRefresh(HalDisplay::HALF_REFRESH);
    startActivityForResult(std::move(activity), [this](const ActivityResult& result) {
      setResult(ActivityResult{result});
      finish();
    });
    return;
  }
  // Name the failure: a genuine miss is "Not found"; a word that WAS found but
  // couldn't be read is a real error — and we distinguish decompression from a
  // low-memory allocation from a generic read error.
  if (!ok) {
    popup = Popup::Error;
    // An index build allocates a scan buffer, so it fails the same way lookups
    // do on a fragmented heap — name that rather than a generic error.
    switch (indexResult) {
      case Dictionary::IndexResult::LowMemory:
        popupMsg = StrId::STR_DICT_LOW_MEMORY;
        break;
      case Dictionary::IndexResult::ReadError:
        popupMsg = StrId::STR_DICT_READ_FAILED;
        break;
      case Dictionary::IndexResult::Ok:
      default:
        popupMsg = StrId::STR_DICT_ERROR;  // dict.open() failed, not the index
        break;
    }
  } else {
    switch (result) {
      case Dictionary::LookupResult::Decompress:
        popup = Popup::Error;
        popupMsg = StrId::STR_DICT_DECOMPRESS_ERROR;
        break;
      case Dictionary::LookupResult::LowMemory:
        popup = Popup::Error;
        popupMsg = StrId::STR_DICT_LOW_MEMORY;
        break;
      case Dictionary::LookupResult::ReadError:
        popup = Popup::Error;
        popupMsg = StrId::STR_DICT_READ_FAILED;
        break;
      case Dictionary::LookupResult::NotFound:
      default:
        popup = Popup::NotFound;
        popupMsg = StrId::STR_DICT_NOT_FOUND;
        break;
    }
  }
  popupTime = millis();
  requestUpdate();
}

void DictionaryWordSelectActivity::recordLookup() {
  READING_STATS.noteEvent(stats::EVENT_LOOKUP);
  if (recordVocabulary && !VocabularyStore::append(vocabulary, recordWord, recordHeadword, recordDefinition)) {
    LOG_ERR("VOCAB", "Could not save successful dictionary lookup");
  }
  dropLookupRecord();
}

void DictionaryWordSelectActivity::dropLookupRecord() {
  recordVocabulary = false;
  std::string{}.swap(recordWord);
  std::string{}.swap(recordHeadword);
  std::string{}.swap(recordDefinition.text);
}

// Same buttons as the definition sheet's Another word / +, in a card at the
// popup position.
DictionaryWordSelectActivity::Box DictionaryWordSelectActivity::notFoundBox(const int part) const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int titleWidth = renderer.getTextWidth(UI_12_FONT_ID, tr(STR_DICT_NOT_FOUND), EpdFontFamily::BOLD);
  const int buttonHeight = (renderer.getLineHeight(UI_10_FONT_ID) + 12) * 11 / 10;
  const int anotherWidth = (renderer.getTextWidth(UI_10_FONT_ID, tr(STR_ANOTHER_WORD)) + 24) * 11 / 10;
  const int closeWidth = (renderer.getTextWidth(UI_10_FONT_ID, tr(STR_CLOSE)) + 24) * 11 / 10;
  const int gap = metrics.contentSidePadding;
  const int rowWidth = anotherWidth + gap + closeWidth;
  const int width = std::max(titleWidth, rowWidth) + 2 * metrics.popupMarginX;
  const int height = renderer.getLineHeight(UI_12_FONT_ID) + buttonHeight + 3 * metrics.popupMarginY;
  const int x = (renderer.getScreenWidth() - width) / 2;
  const int y = static_cast<int>(renderer.getScreenHeight() * metrics.popupTopOffsetRatio);
  if (part < 0) return {x, y, width, height};
  const int rowX = x + (width - rowWidth) / 2;
  const int rowY = y + height - metrics.popupMarginY - buttonHeight;
  if (part == 0) return {rowX, rowY, anotherWidth, buttonHeight};
  return {rowX + anotherWidth + gap, rowY, closeWidth, buttonHeight};
}

void DictionaryWordSelectActivity::drawNotFoundButton(const int index, const bool pressed) const {
  const Box b = notFoundBox(index);
  drawLookupButton(renderer, Rect{b.x, b.y, b.width, b.height}, index == 0 ? tr(STR_ANOTHER_WORD) : tr(STR_CLOSE),
                   pressed);
}

void DictionaryWordSelectActivity::drawNotFound() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Box card = notFoundBox(-1);
  const int frame = std::max(1, static_cast<int>(metrics.popupFrameThickness));
  const int outerRadius = metrics.popupCornerRadius > 0 ? metrics.popupCornerRadius + frame : 0;
  renderer.fillRoundedRect(card.x - frame, card.y - frame, card.width + 2 * frame, card.height + 2 * frame, outerRadius,
                           Color::Black);
  renderer.fillRoundedRect(card.x, card.y, card.width, card.height, metrics.popupCornerRadius, Color::White);
  const char* title = tr(STR_DICT_NOT_FOUND);
  const int titleX = card.x + (card.width - renderer.getTextWidth(UI_12_FONT_ID, title, EpdFontFamily::BOLD)) / 2;
  renderer.drawText(UI_12_FONT_ID, titleX, card.y + metrics.popupMarginY, title, true, EpdFontFamily::BOLD);
  drawNotFoundButton(0, false);
  drawNotFoundButton(1, false);
}

// Pressed state first (one quick refresh), then Another word goes back to word
// selection on the same page; Close goes back to the book.
void DictionaryWordSelectActivity::closeNotFound(const int index, const bool showPressed) {
  {
    RenderLock lock;
    if (showPressed) {
      drawNotFoundButton(index, true);
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    }
    // The card and the pressed button leave ghosting: the next screen gets
    // one clean (HALF) refresh.
    renderer.promoteNextRefresh(HalDisplay::HALF_REFRESH);
  }
  if (index == 0) {
    LookupFollowUpResult followUp;
    followUp.kind = LookupFollowUpResult::Kind::AnotherWord;
    setResult(ActivityResult{followUp});
  }
  finish();
}

void DictionaryWordSelectActivity::handleNotFoundInput() {
  int tx = 0;
  int ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty)) {
    constexpr int SLOP = 8;  // small targets: a little extra around each
    for (int i = 0; i < 2; ++i) {
      const Box b = notFoundBox(i);
      if (tx >= b.x - SLOP && tx < b.x + b.width + SLOP && ty >= b.y - SLOP && ty < b.y + b.height + SLOP) {
        closeNotFound(i, true);
        return;
      }
    }
    // A tap on the page outside the card closes it, like a tap above the definition sheet.
    const Box card = notFoundBox(-1);
    if (tx < card.x || tx >= card.x + card.width || ty < card.y || ty >= card.y + card.height) closeNotFound(1, false);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    closeNotFound(0, true);
    return;
  }
  // Under the touch lock the side Down button backs out, as in word selection.
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      (lookupPausesTouchLock && mappedInput.wasReleased(MappedInputManager::Button::Down))) {
    closeNotFound(1, true);
  }
}

void DictionaryWordSelectActivity::loop() {
  if (lookupPending) {
    lookupPending = false;
    performLookup();
    return;
  }
  if (popup == Popup::NotFound) {
    handleNotFoundInput();
    return;
  }
  if (popup == Popup::Error && millis() - popupTime >= POPUP_DURATION_MS) {
    result.isCancelled = true;
    // The pressed button and the dark popup leave ghosting: let the
    // book's next paint be a clean (HALF) refresh.
    renderer.promoteNextRefresh(HalDisplay::HALF_REFRESH);
    finish();
  }
}

void DictionaryWordSelectActivity::render(RenderLock&&) {
  renderer.clearScreen();

  auto* fcm = renderer.getFontCacheManager();
  auto scope = fcm->createPrewarmScope();
  if (page) page->render(renderer, fontId, marginLeft, marginTop);
  scope.endScanAndPrewarm();
  if (page) page->render(renderer, fontId, marginLeft, marginTop);

  if (popup == Popup::NotFound) {
    drawNotFound();
    renderer.displayBuffer();
    return;
  }
  if (popup != Popup::None) {
    GUI.drawPopup(renderer, I18N.get(popupMsg));
    return;
  }
  if (!paintOnly) renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
