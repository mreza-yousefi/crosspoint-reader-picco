#pragma once

#include <cstdint>
#include <string>

// Home-server backup settings and last-run state, in /.crosspoint/backup.json.
// The password is XOR-obfuscated with the device key, like the KOSync store.
struct BackupConfig {
  static constexpr int64_t AUTO_INTERVAL_SECONDS = 24 * 60 * 60;

  std::string serverUrl = "";
  std::string username;
  std::string password;
  // Where to browse the backup in a web browser (shown in settings only).
  std::string browseUrl = "";
  bool autoBackup = true;
  int64_t lastSuccessEpoch = 0;  // 0 = never, or unknown clock at that time
  int lastResult = 0;            // BackupEngine::Result of the last run

  bool isConfigured() const { return !serverUrl.empty() && !username.empty() && !password.empty(); }
  bool load();
  bool save() const;
};
