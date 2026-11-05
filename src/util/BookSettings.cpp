#include "BookSettings.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <PersistableStore.h>

#include "CrossPointSettings.h"

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
  // A missing or unreadable file: create it from the current values. The
  // directory is made only here; save() runs after this for the same book.
  if (!PersistableStoreBase::readDocFromFile(settingsPath.c_str(), doc)) {
    Storage.mkdir(BOOK_SETTINGS_DIR);
    save(settingsPath);
    return;
  }
  // Persist so a book opened next starts from these values ("last used").
  if (SETTINGS.readingSettingsFromJson(doc.as<JsonVariantConst>())) SETTINGS.saveToFile();
}

bool save(const std::string& settingsPath) {
  if (settingsPath.empty()) return true;
  JsonDocument doc;
  SETTINGS.readingSettingsToJson(doc);
  return PersistableStoreBase::writeDocToFile(settingsPath.c_str(), doc);
}

void persist(const std::string& settingsPath) {
  SETTINGS.saveToFile();
  save(settingsPath);
}

}  // namespace booksettings
