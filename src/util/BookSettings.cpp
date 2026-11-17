#include "BookSettings.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <PersistableStore.h>

#include <cstring>

#include "BookSettingsMerge.h"
#include "CrossPointSettings.h"
#include "TaskWatchdog.h"

namespace {
constexpr char BOOK_SETTINGS_DIR[] = "/.crosspoint/book-settings";
}  // namespace

namespace booksettings {

std::string pathForDocument(const std::string& documentId) {
  if (documentId.empty()) return "";
  return std::string(BOOK_SETTINGS_DIR) + "/" + documentId + ".json";
}

void applyOrCreate(const std::string& settingsPath) {
  if (settingsPath.empty()) return;
  JsonDocument doc;
  // A missing or unreadable file: create it from the defaults SETTINGS still
  // holds. The directory is made only here; save() runs after this for the same book.
  if (!PersistableStoreBase::readDocFromFile(settingsPath.c_str(), doc)) {
    Storage.mkdir(BOOK_SETTINGS_DIR);
    save(settingsPath);
    return;
  }
  SETTINGS.readingSettingsFromJson(doc.as<JsonVariantConst>());
}

bool save(const std::string& settingsPath) {
  if (settingsPath.empty()) return true;
  JsonDocument doc;
  SETTINGS.readingSettingsToJson(doc);
  return PersistableStoreBase::writeDocToFile(settingsPath.c_str(), doc);
}

void persist(const std::string& settingsPath) {
  if (settingsPath.empty()) {
    SETTINGS.saveToFile();
  } else {
    save(settingsPath);
  }
}

int applyChangesToAllBooks(JsonObjectConst before, JsonObjectConst after) {
  HalFile dir = Storage.open(BOOK_SETTINGS_DIR);
  if (!dir || !dir.isDirectory()) return 0;

  int updated = 0;
  int skipped = 0;
  char name[64];
  for (;;) {
    bool isSettingsFile;
    {
      // Closed before the file is rewritten (the write replaces it).
      HalFile entry = dir.openNextFile();
      if (!entry) break;
      const size_t len = entry.getName(name, sizeof(name));
      isSettingsFile = !entry.isDirectory() && name[0] != '.' && len > 5 && strcmp(name + len - 5, ".json") == 0;
    }
    if (!isSettingsFile) continue;
    resetTaskWatchdogIfSubscribed();

    // A rewritten file can be listed again; the merge then finds nothing to write.
    const std::string path = std::string(BOOK_SETTINGS_DIR) + "/" + name;
    JsonDocument doc;
    if (!PersistableStoreBase::readDocFromFile(path.c_str(), doc) || !doc.is<JsonObject>()) {
      LOG_ERR("BKS", "Skipping unreadable %s", path.c_str());
      skipped++;
      continue;
    }
    if (mergeChangedKeys(doc.as<JsonObject>(), before, after) == 0) continue;
    if (PersistableStoreBase::writeDocToFile(path.c_str(), doc)) {
      updated++;
    } else {
      skipped++;
    }
  }
  LOG_INF("BKS", "Applied text settings to %d books, %d skipped", updated, skipped);
  return skipped;
}

}  // namespace booksettings
