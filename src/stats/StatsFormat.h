#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// On-disk records of the reading statistics. The contract (paths, versions, field meaning)
// is docs/reading-stats.md; change both together. All integers are little-endian; every
// struct has natural alignment and no padding (checked below), so it is written as is.
namespace stats {

constexpr const char* STATS_DIR = "/.crosspoint/stats";
constexpr const char* BOOKS_LOG_DIR = "/.crosspoint/stats/books";
constexpr const char* REGISTRY_PATH = "/.crosspoint/stats/books.bin";
constexpr const char* SESSIONS_PATH = "/.crosspoint/stats/sessions.bin";
constexpr const char* TOTALS_PATH = "/.crosspoint/stats/booktotals.bin";
constexpr const char* DAYS_PATH = "/.crosspoint/stats/days.bin";
constexpr const char* CURRENT_PATH = "/.crosspoint/stats/current.bin";

constexpr uint16_t RAW_LOG_VERSION = 1;
constexpr uint16_t REGISTRY_VERSION = 1;
constexpr uint16_t SESSIONS_VERSION = 1;
constexpr uint16_t TOTALS_VERSION = 1;
constexpr uint16_t DAYS_VERSION = 1;
constexpr uint16_t CURRENT_VERSION = 1;

inline constexpr char MAGIC_LOG[4] = {'C', 'P', 'R', 'L'};
inline constexpr char MAGIC_REGISTRY[4] = {'C', 'P', 'B', 'R'};
inline constexpr char MAGIC_SESSIONS[4] = {'C', 'P', 'S', 'I'};
inline constexpr char MAGIC_TOTALS[4] = {'C', 'P', 'B', 'T'};
inline constexpr char MAGIC_DAYS[4] = {'C', 'P', 'D', 'Y'};
inline constexpr char MAGIC_CURRENT[4] = {'C', 'P', 'C', 'S'};

constexpr int RATE_BUCKETS = 40;
constexpr uint16_t NO_BOOK = 0xFFFF;
constexpr size_t DOC_ID_LEN = 32;

// Common 16-byte header of every stats file.
struct FileHeader {
  char magic[4];
  uint16_t version;
  uint16_t headerSize;
  uint16_t recordSize;
  uint8_t flags;  // booktotals.bin, days.bin: SUMMARY_FLAG_*; 0 elsewhere
  uint8_t reserved0;
  uint32_t reserved1;
};
static_assert(sizeof(FileHeader) == 16);

inline bool headerMatches(const FileHeader& header, const char (&magic)[4], const uint16_t version,
                          const uint16_t headerSize, const uint16_t recordSize) {
  return memcmp(header.magic, magic, sizeof(header.magic)) == 0 && header.version == version &&
         header.headerSize == headerSize && header.recordSize == recordSize;
}

// --- Raw per-book log: books/<document id>.log --------------------------------------------
// FileHeader "CPRL", then records. Each starts with its type and its size in bytes, so a
// reader skips types it does not know and reads the known prefix of a longer record. The
// format grows by new types and by fields appended to a type, never by changing a field.

enum RawType : uint8_t { RAW_START = 'S', RAW_TURN = 'T', RAW_EVENT = 'E', RAW_END = 'X' };
enum TurnKind : uint8_t { TURN_NEXT = 1, TURN_PREV = 2, TURN_JUMP_FWD = 3, TURN_JUMP_BACK = 4 };
// Pause/resume: a menu or the stats screen over the page (that time is not reading).
enum EventKind : uint8_t {
  EVENT_LOOKUP = 1,
  EVENT_CLIPPING = 2,
  EVENT_BOOKMARK = 3,
  EVENT_END_OF_BOOK = 4,
  EVENT_PAUSE = 5,
  EVENT_RESUME = 6
};
enum EndReason : uint8_t { END_EXIT = 1, END_SLEEP = 2, END_AUTO_SLEEP = 3, END_CRASH = 4 };

enum SessionFlag : uint8_t {
  SESSION_WALL_KNOWN = 1 << 0,  // startUtc is trusted wall time
  SESSION_SHORT = 1 << 1,       // fails the short-open rule (isShortOpen)
  SESSION_CRASHED = 1 << 2,     // closed by recovery, not by the reader
  SESSION_LIGHT_ON = 1 << 3,    // frontlight on at session start
  SESSION_FINISHED = 1 << 4,    // crossed FINISHED_BP or reached the end-of-book screen
};

struct RawStart {
  uint8_t type;   // RAW_START
  uint8_t size;   // sizeof(RawStart)
  uint8_t flags;  // SESSION_WALL_KNOWN, SESSION_LIGHT_ON
  uint8_t fontSize;
  uint16_t spine;
  int16_t tzOffsetMin;
  uint32_t startUtc;  // 0 = wall time unknown
  uint32_t offset;    // visible text offset in the spine item, UNKNOWN_OFFSET when unknown
  uint16_t percentBp;
  uint16_t paperPage;  // 0 = unknown
  uint16_t paperPagesTotal;
  uint8_t orientation;
  uint8_t reserved;
  uint32_t sessionSeq;
  uint32_t tMs;  // first page shown, since session start
};
static_assert(sizeof(RawStart) == 32);

struct RawTurn {
  uint8_t type;  // RAW_TURN
  uint8_t size;
  uint8_t kind;  // TurnKind
  uint8_t reserved;
  uint16_t spine;
  uint16_t percentBp;
  uint32_t tMs;  // since session start
  uint32_t offset;
  uint16_t paperPage;
  uint16_t reserved2;
};
static_assert(sizeof(RawTurn) == 20);

struct RawEvent {
  uint8_t type;  // RAW_EVENT
  uint8_t size;
  uint8_t kind;  // EventKind
  uint8_t reserved;
  uint32_t tMs;
};
static_assert(sizeof(RawEvent) == 8);

struct RawEnd {
  uint8_t type;  // RAW_END
  uint8_t size;
  uint8_t reason;  // EndReason
  uint8_t flags;   // SessionFlag
  uint32_t durationMs;
  uint32_t activeSec;
  uint32_t endUtc;     // 0 = wall time unknown at the end
  uint32_t charsRead;  // text of next pages in the same chapter (exact offset deltas)
  uint32_t learnedMs;  // active time on those pages
  uint16_t turnsFwd;
  uint16_t turnsBack;
  uint16_t jumps;
  uint16_t lookups;
  uint16_t clippings;
  uint16_t bookmarks;
  uint16_t endPaperPage;
  uint16_t endPercentBp;
  uint16_t endSpine;
  int16_t tzOffsetMin;  // at endUtc
  uint32_t reserved;
};
static_assert(sizeof(RawEnd) == 48);

constexpr size_t RAW_MAX_KNOWN_SIZE = sizeof(RawEnd);

// Size of each type in version 1. Fixed forever: a reader accepts any record at least this
// long, so a log written before a field was appended still reads.
inline size_t rawMinSize(const uint8_t type) {
  switch (type) {
    case RAW_START:
      return 32;
    case RAW_TURN:
      return 20;
    case RAW_EVENT:
      return 8;
    case RAW_END:
      return 48;
    default:
      return 0;  // unknown type
  }
}
static_assert(sizeof(RawStart) >= 32 && sizeof(RawTurn) >= 20 && sizeof(RawEvent) >= 8 && sizeof(RawEnd) >= 48);

// Copies a framed record into its struct. Fields the record is too short for (appended to the
// type after it was written) read as 0, so a new field must mean "not known" at 0. False for an
// unknown type or a record shorter than its version 1 size.
inline bool readRawRecord(const uint8_t* record, const size_t size, void* out, const size_t outSize) {
  const size_t minSize = size >= 1 ? rawMinSize(record[0]) : 0;
  if (minSize == 0 || size < minSize) return false;
  memset(out, 0, outSize);
  memcpy(out, record, size < outSize ? size : outSize);
  return true;
}

// --- Session index: sessions.bin, FileHeader "CPSI" then SessionRecord[] -----------------

struct SessionRecord {
  uint32_t startUtc;  // 0 = wall time unknown
  uint32_t durationSec;
  uint32_t activeSec;
  uint16_t bookIndex;   // record number in books.bin
  int16_t tzOffsetMin;  // local = UTC + offset, at session start
  uint16_t turnsFwd;
  uint16_t turnsBack;
  uint16_t jumps;
  uint16_t lookups;
  uint16_t clippings;
  uint16_t bookmarks;
  uint16_t startPercentBp;
  uint16_t endPercentBp;
  uint16_t startPaperPage;
  uint16_t endPaperPage;
  uint16_t startSpine;
  uint16_t endSpine;
  uint8_t flags;  // SessionFlag
  uint8_t endReason;
  uint8_t fontSize;
  uint8_t orientation;
  uint32_t sessionSeq;
  uint32_t charsRead;  // as in RawEnd: reading speed = charsRead / learnedMs
  uint32_t learnedMs;
};
static_assert(sizeof(SessionRecord) == 56);

// --- Book registry: books.bin, RegistryHeader then BookRecord[] (primary data only) ------

struct RegistryHeader {
  FileHeader file;  // "CPBR", recordSize = sizeof(BookRecord)
  uint32_t nextSessionSeq;
  uint32_t globalSamples;
  uint16_t globalHist[RATE_BUCKETS];  // all books' reading rates (see addRateSample)
  uint8_t reserved[24];
};
static_assert(sizeof(RegistryHeader) == 128);

enum BookFlag : uint32_t { BOOK_EXCLUDED = 1u << 0 };

struct BookRecord {
  char docId[DOC_ID_LEN];  // KOReader document id, lowercase hex, not terminated
  uint32_t flags;          // BookFlag
  uint32_t rateSamples;
  uint16_t rateHist[RATE_BUCKETS];
  uint16_t typicalPageChars;
  uint16_t reserved;
  uint8_t reserved2[36];
  char title[120];  // UTF-8, zero-terminated (truncated)
  char author[56];
  char path[176];
};
static_assert(sizeof(BookRecord) == 512);

// --- Book totals: booktotals.bin, FileHeader "CPBT" then BookTotals[] by book index --------
// Derived from sessions.bin: the short-open rule applies, exclusion does not.

enum SummaryFlag : uint8_t { SUMMARY_FLAG_IGNORE_SHORT = 1 << 0 };

struct BookTotals {
  uint32_t firstUtc;  // first session start with known wall time, 0 = none
  uint32_t lastUtc;
  uint32_t lastEndUtc;  // for the sitting rule
  uint32_t totalActiveSec;
  uint32_t sessions;
  uint32_t sittings;
  uint32_t finishedUtc;  // session that crossed FINISHED_BP; 1 = time unknown
  uint16_t maxPercentBp;
  uint16_t lastPercentBp;
  uint32_t charsRead;
  uint32_t learnedMs;
};
static_assert(sizeof(BookTotals) == 40);

// --- Day totals: days.bin, DaysHeader then DayRecord[] ascending by day -------------------

struct DaysHeader {
  FileHeader file;                // "CPDY", flags = SummaryFlag the totals were built with
  uint32_t unknownTimeActiveSec;  // counted sessions without wall time (not in any day)
  uint32_t reserved[3];
};
static_assert(sizeof(DaysHeader) == 32);

struct DayRecord {
  int32_t day;  // local days since 1970-01-01
  uint32_t activeSec;
  uint16_t sessions;
  uint16_t sittings;
  uint16_t pagesRead;  // paper pages moved forward
  uint16_t reserved;
};
static_assert(sizeof(DayRecord) == 16);

// --- In-progress session: current.bin -----------------------------------------------------

struct CurrentRecord {
  FileHeader file;  // "CPCS"
  char docId[DOC_ID_LEN];
  SessionRecord session;  // totals so far
};
static_assert(sizeof(CurrentRecord) == 104);

}  // namespace stats
