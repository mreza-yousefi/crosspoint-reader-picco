#include "DictionaryWordSelectActivity.h"

#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Memory.h>
#include <TrustedTime.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <ctime>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "DictionaryDefinitionActivity.h"
#include "components/UITheme.h"
#include "util/HtmlToPlainText.h"

namespace {

constexpr unsigned long POPUP_DURATION_MS = 1500;

void indexBuildYield(void*) { vTaskDelay(1); }

// Appends one TSV line per successful lookup to /vocabulary/lookups.tsv, for
// reviewing new words later (the home backup copies it too). One SD append.
void recordLookup(const std::string& word, const std::string& headword, const std::string& definition,
                  const bool html) {
  constexpr size_t MAX_DEF = 160;
  const auto clean = [](std::string s) {
    for (auto& c : s) {
      if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    }
    return s;
  };
  std::string def = html ? htmlToPlainText(definition.substr(0, 2048), ' ') : definition.substr(0, 2048);
  if (def.size() > MAX_DEF) {
    size_t cut = MAX_DEF;
    while (cut > 0 && (static_cast<unsigned char>(def[cut]) & 0xC0) == 0x80) --cut;  // UTF-8 boundary
    def.resize(cut);
  }
  char when[24] = "";
  const time_t now = static_cast<time_t>(trustedtime::trustedNow());
  if (now > 0) {
    struct tm local{};
    localtime_r(&now, &local);
    strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &local);
  }
  std::string book = APP_STATE.openEpubPath;
  const size_t slash = book.rfind('/');
  if (slash != std::string::npos) book.erase(0, slash + 1);
  Storage.ensureDirectoryExists("/vocabulary");
  const bool fresh = !Storage.exists("/vocabulary/lookups.tsv");
  HalFile f = Storage.open("/vocabulary/lookups.tsv", O_WRITE | O_CREAT | O_APPEND);
  if (!f) return;
  if (fresh) f.print("date\tword\theadword\tbook\tdefinition\n");
  const std::string line =
      std::string(when) + '\t' + clean(word) + '\t' + clean(headword) + '\t' + clean(book) + '\t' + clean(def) + '\n';
  f.write(reinterpret_cast<const uint8_t*>(line.data()), line.size());
  f.close();
}

}  // namespace

void DictionaryWordSelectActivity::onEnter() {
  Activity::onEnter();
  fontId = SETTINGS.getReaderFontId();
  lookupPending = true;
  popupMsg = StrId::STR_DICT_LOOKING_UP;
  popup = Popup::Busy;
  requestUpdate();
}

void DictionaryWordSelectActivity::performLookup() {
  {
    RenderLock lock;
    popupMsg = StrId::STR_DICT_LOOKING_UP;
    popup = Popup::Busy;
  }
  if (!dictOpenAttempted) {
    dictOpenAttempted = true;
    dictOpenOk = dict.open(SETTINGS.dictionaryName);
    // needsIndex() opens and validates the .qidx sidecar, so ask it once per
    // open rather than once per word: the answer only changes when we build
    // the sidecar ourselves, which is handled below.
    dictNeedsIndex = dictOpenOk && dict.needsIndex();
  }
  {
    RenderLock lock;
    popupMsg = dictNeedsIndex ? StrId::STR_DICT_INDEXING : StrId::STR_DICT_LOOKING_UP;
  }
  requestUpdateAndWait();  // paint the page + busy popup before blocking on SD

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
    recordLookup(lookupText, headword, definition, dict.definitionsAreHtml());
    {
      RenderLock lock;
      popup = Popup::None;
    }
    // The definition sheet keeps the frame above it: repaint the page without
    // the "Looking up..." popup first.
    requestUpdateAndWait();
    {
      RenderLock lock;
      page.reset();
    }
    auto activity = makeUniqueNoThrow<DictionaryDefinitionActivity>(renderer, mappedInput, std::move(headword),
                                                                    std::move(definition), dict.definitionsAreHtml());
    if (!activity || !activity->enableLookup(dict, std::move(lookupText))) {
      LOG_ERR("DICT", "OOM: definition activity");
      popup = Popup::Error;
      popupMsg = StrId::STR_DICT_LOW_MEMORY;
      popupTime = millis();
      requestUpdate();
      return;
    }
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

void DictionaryWordSelectActivity::loop() {
  if (lookupPending) {
    lookupPending = false;
    performLookup();
    return;
  }
  if (popup == Popup::NotFound || popup == Popup::Error) {
    if (millis() - popupTime >= POPUP_DURATION_MS) {
      result.isCancelled = popup == Popup::Error;
      finish();
    }
  }
}

void DictionaryWordSelectActivity::render(RenderLock&&) {
  renderer.clearScreen();

  auto* fcm = renderer.getFontCacheManager();
  auto scope = fcm->createPrewarmScope();
  if (page) page->render(renderer, fontId, marginLeft, marginTop);
  scope.endScanAndPrewarm();
  if (page) page->render(renderer, fontId, marginLeft, marginTop);

  if (popup != Popup::None) {
    GUI.drawPopup(renderer, I18N.get(popupMsg));
    return;
  }
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
