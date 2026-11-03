#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

struct BackupConfig;

// Incremental push of the SD card to a WebDAV server (home backup target).
// Each run first fetches books waiting in the server's /to-reader/ folder into
// /books and deletes them there.
// Uploads only files whose size, date or (for small files) CRC changed since
// the last successful upload, tracked in /.crosspoint/backup/manifest.tsv.
// Never deletes on the server. Caller owns WiFi: it must already be connected.
namespace backup {

enum class Result : int {
  Ok = 0,
  Partial = 1,  // time budget ran out; the rest goes next run
  NotConfigured = 2,
  NetworkError = 3,
  AuthFailed = 4,
  ServerError = 5,
  StorageError = 6,
  NoCertificate = 7,  // https:// without the pinned certificate file on the SD card
};

struct Progress {
  size_t filesDone = 0;     // changed files uploaded so far
  size_t filesPending = 0;  // changed files found by the scan
  uint64_t bytesDone = 0;
  size_t filesFetched = 0;  // books fetched from the server's to-reader folder
};

// Called after the scan and after each uploaded file. Return false to stop.
using ProgressCallback = std::function<bool(const Progress&)>;

// Ok when a run can start: credentials set and, for https://, the pinned
// certificate file present. Local checks only (no network).
Result ready(const BackupConfig& config);

// budgetMs = 0 means no time limit.
Result run(const BackupConfig& config, uint32_t budgetMs, const ProgressCallback& onProgress);

// run(), then store the result and, on success, the time in the config file.
Result runAndRecord(BackupConfig& config, uint32_t budgetMs, const ProgressCallback& onProgress);

// Seconds since the last successful backup, or -1 when unknown (never, or no clock).
int64_t secondsSinceLastBackup(const BackupConfig& config);

const char* resultLabel(Result result);

}  // namespace backup
