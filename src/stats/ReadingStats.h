#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "StatsFormat.h"
#include "StatsLogic.h"

// Records EPUB reading sessions and keeps the summaries under /.crosspoint/stats
// (docs/reading-stats.md). The reader calls beginSession/endSession, setPaused and tick on
// its loop task and onPageShown/noteEndOfBook on the render task; page data is buffered in
// RAM and written by tick() in idle frames and at session end, never on the page-turn path.
class ReadingStats {
 public:
  static ReadingStats& getInstance();

  struct PageInfo {
    stats::PagePos pos;
    uint16_t paperPage = 0;  // 0 = not counted yet
    uint16_t paperPagesTotal = 0;
    uint16_t percentBp = 0;
  };
  enum class Hidden : uint8_t { Unknown, Shown, Hidden };

  // Loop task. Starts a session for a book with a KOReader document id; ends a running one.
  void beginSession(const std::string& docId, const std::string& path, const std::string& title,
                    const std::string& author);
  // Render task, after a full render put a page on the panel. No I/O.
  void onPageShown(const PageInfo& info);
  // Render task: the end-of-book screen is showing (100%).
  void noteEndOfBook();
  void noteEvent(stats::EventKind kind);
  // Loop task: a menu or the stats screen is over the page; that time is not reading.
  void setPaused(bool paused);
  // Loop task, idle frames: writes buffered records when due.
  void tick();
  void endSession(stats::EndReason reason);

  // The running session's totals so far (activeSec includes the open dwell); false when none.
  bool liveSession(stats::SessionRecord& out, bool& bookExcluded) const;

  // Closes a session left by a reset, then rebuilds stale or missing summaries. Call before
  // reading them.
  void prepareSummaries();
  // Rebuilds days.bin and booktotals.bin from sessions.bin.
  bool rebuildSummaries();
  // Rebuilds sessions.bin from the raw book logs, then the summaries.
  bool rebuildFromLogs();

  Hidden hiddenState(const std::string& docId);
  // Hides or shows a book in the calendar, then rebuilds the summaries.
  bool setHidden(const std::string& docId, bool hidden);
  // Hidden flag per book index; empty without a registry.
  bool loadHiddenFlags(std::vector<uint8_t>& out);
  bool bookTitle(int index, char* out, size_t size);

 private:
  ReadingStats();

  static constexpr uint32_t FLUSH_INTERVAL_MS = 3 * 60 * 1000;
  // First snapshot: after the first page turn, or this long into the session.
  static constexpr uint32_t FIRST_SNAPSHOT_MS = 60 * 1000;
  // Raw records buffered between flushes: 2 KB holds 100 page turns.
  static constexpr size_t BUFFER_BYTES = 2048;
  static constexpr size_t FLUSH_AT_BYTES = BUFFER_BYTES / 2;

  struct Session {
    char docId[stats::DOC_ID_LEN] = {};
    uint16_t bookIndex = stats::NO_BOOK;
    bool bookExcluded = false;
    uint32_t startMs = 0;
    uint32_t lastFlushMs = 0;
    bool startWritten = false;
    bool snapshotWritten = false;
    bool dirty = false;  // changed since the last snapshot
    stats::SessionRecord record{};
    stats::RawStart start{};
    stats::ActiveClock clock;
    // Learned this session: the book model (copied from the registry at begin) and the
    // global histogram's additions.
    uint16_t bookHist[stats::RATE_BUCKETS] = {};
    uint32_t bookSamples = 0;
    uint16_t globalAdd[stats::RATE_BUCKETS] = {};
    uint32_t globalAddSamples = 0;
    uint8_t pending[BUFFER_BYTES];
    size_t pendingLen = 0;
    uint32_t droppedRecords = 0;
    // Copy target for the flush, so the SD write runs without the mutex.
    uint8_t scratch[BUFFER_BYTES];
  };

  // Sorted by key for lookups by document id; built from books.bin once per boot.
  struct IdEntry {
    uint64_t key;  // first 16 hex digits of the id; equal keys are told apart by reading the id
    uint16_t index;
  };

  class Lock {
   public:
    explicit Lock(const ReadingStats& owner) : owner(owner) { xSemaphoreTake(owner.mutex, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(owner.mutex); }

   private:
    const ReadingStats& owner;
  };

  // Keeps room for the end record, so a full buffer still closes the session in the log.
  void appendPending(Session& session, const void* record, size_t size, bool isEnd = false);
  void appendEvent(Session& session, stats::EventKind kind, uint32_t nowMs);
  // Writes the buffered records and the current.bin snapshot. Loop task only.
  void flush(Session& session, uint32_t nowMs);
  void finalize(const char* docId, uint16_t bookIndex, const stats::SessionRecord& record, const Session* learned);
  void recoverCrashedSession();
  enum class AppendResult : uint8_t { Ok, BadHeader, WriteFailed };
  AppendResult appendSession(const stats::SessionRecord& record);
  bool updateTotals(uint16_t bookIndex, bool excluded, const stats::SessionRecord& record, stats::ApplyResult& result);
  bool updateDays(const stats::DayRecord& add, uint32_t unknownTimeAdd);
  // Removes the derived summaries; prepareSummaries() rebuilds them.
  void invalidateSummaries();
  bool summariesStale();
  bool sessionsUsable();
  int registerBook(const std::string& docId, const std::string& path, const std::string& title,
                   const std::string& author, stats::BookRecord& record, stats::RegistryHeader& header,
                   uint32_t& sessionSeq);
  // Builds idCache and hiddenByIndex from books.bin when missing or of another size.
  bool loadIdCache();
  int findBookIndex(const char* docId);
  bool bookExcluded(uint16_t index);

  mutable StaticSemaphore_t mutexBuffer;
  SemaphoreHandle_t mutex;
  // Owned by the loop task; the render task reaches it only under the mutex.
  std::unique_ptr<Session> session;
  // Mirrors the clock's pause state so setPaused() is a cheap no-op on most loop frames.
  std::atomic<bool> pausedFlag{false};
  // Loop task only. 17 bytes per book, kept for the uptime once a book or the stats were opened.
  std::vector<IdEntry> idCache;
  std::vector<uint8_t> hiddenByIndex;  // BOOK_EXCLUDED per registry record
};

#define READING_STATS ReadingStats::getInstance()
