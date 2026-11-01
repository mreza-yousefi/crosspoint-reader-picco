#include "BackupConfig.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <ObfuscationUtils.h>

namespace {
constexpr char CONFIG_PATH[] = "/.crosspoint/backup.json";
constexpr size_t CONFIG_MAX_BYTES = 2048;
}  // namespace

bool BackupConfig::load() {
  std::string text;
  if (!Storage.readFileToString("BACKUP", CONFIG_PATH, CONFIG_MAX_BYTES, text)) return false;
  JsonDocument doc;
  if (deserializeJson(doc, text)) {
    LOG_ERR("BACKUP", "Bad %s", CONFIG_PATH);
    return false;
  }
  serverUrl = doc["url"] | serverUrl.c_str();
  username = doc["user"] | username.c_str();
  password = obfuscation::deobfuscateFromBase64(doc["password_obf"] | "");
  autoBackup = doc["auto"] | autoBackup;
  lastSuccessEpoch = doc["last_ok"] | static_cast<int64_t>(0);
  lastResult = doc["last_result"] | 0;
  return true;
}

bool BackupConfig::save() const {
  JsonDocument doc;
  doc["url"] = serverUrl;
  doc["user"] = username;
  doc["password_obf"] = obfuscation::obfuscateToBase64(password);
  doc["auto"] = autoBackup;
  doc["last_ok"] = lastSuccessEpoch;
  doc["last_result"] = lastResult;
  String out;
  serializeJson(doc, out);
  Storage.ensureDirectoryExists("/.crosspoint");
  return Storage.writeFile(CONFIG_PATH, out);
}
