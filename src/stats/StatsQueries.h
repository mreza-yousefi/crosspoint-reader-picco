#pragma once

#include <HalStorage.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "StatsExport.h"
#include "StatsFormat.h"
#include "StatsInsights.h"
#include "StatsLogic.h"

// Read side of the reading statistics for the screens. Reads the summaries (days.bin,
// booktotals.bin); the day list streams sessions.bin. Call ReadingStats::prepareSummaries()
// first.
namespace stats {

// Today's local day; known is false when the clock is not trusted (then 0).
int32_t today(bool& known);

struct DayTotals {
  std::vector<DayRecord> days;  // ascending
  uint32_t unknownTimeSec = 0;  // counted reading without wall time
};
// days.bin plus the running session, when it counts.
bool loadDayTotals(DayTotals& out);

// Streams the calendar days in day order (days.bin with the running session merged in) without
// keeping them. unknownTimeSec: counted reading without wall time. Stops early once *stop is set.
using DayFn = void (*)(void* ctx, const DayRecord& day);
void forEachDay(DayFn fn, void* ctx, uint32_t& unknownTimeSec, const bool* stop = nullptr);

// Calls fn for every session in sessions.bin in file order, then the running session
// (live = true); hidden: the book is hidden from the calendar. Stops early once *stop is set.
using AnySessionFn = void (*)(void* ctx, const SessionRecord& session, bool live, bool hidden);
void forEachSession(AnySessionFn fn, void* ctx, const bool* stop = nullptr);

// Calls fn for every session that counts in the calendar (countsInCalendar), from
// sessions.bin in file order, then the running session (live = true).
using SessionFn = void (*)(void* ctx, const SessionRecord& session, bool live);
void forEachCalendarSession(SessionFn fn, void* ctx);

struct DaySession {
  SessionRecord record;
  bool live = false;
};
// Calendar sessions that started on a local day.
bool loadDaySessions(int32_t day, std::vector<DaySession>& out);

// Books first finished in a local year (the session that crossed FINISHED_BP), hidden books left out.
int countFinishedInYear(int year);

struct BookListEntry {
  uint32_t lastUtc = 0;  // last dated session start, 0 = none
  int32_t lastDay = 0;   // its local day (the session's own offset), when lastUtc != 0
  uint32_t activeSec = 0;
  uint16_t index = 0;  // in books.bin
  ReadingStyle style = ReadingStyle::None;
  bool counted = false;  // has a counted session
};
// The Books list (books shown in the stats, with counted reading) or the hidden books, last
// read first; the running session included.
bool loadBookList(bool hiddenBooks, std::vector<BookListEntry>& out);

// Title and author by book index from books.bin, kept open; the last book read is cached.
class BookNames {
 public:
  // False without a registry.
  bool open();
  // Empty strings for an unknown index.
  void load(uint16_t index);
  const char* title() const { return text.title; }
  const char* author() const { return text.author; }

 private:
  struct Text {
    char title[sizeof(BookRecord::title)];
    char author[sizeof(BookRecord::author)];
  };
  HalFile file;
  int count = 0;
  int loaded = -1;
  Text text = {};
};

// Books with counted reading and hidden books, in registry order, with their booktotals.bin
// record and the Books list's style. fn returns false to stop.
using BookRowFn = bool (*)(void* ctx, const BookExport& book);
bool forEachBook(BookRowFn fn, void* ctx);

struct BookSessionRow {
  uint32_t startUtc;  // 0 = wall time unknown
  uint32_t durationSec;
  uint32_t activeSec;
  uint32_t dictSec;
  int16_t tzOffsetMin;
  uint16_t startPercentBp;
  uint16_t endPercentBp;
  uint16_t lookups;
  uint8_t flags;  // SessionFlag
  bool live;
};
// The newest counted sessions a book screen lists (28 bytes each).
constexpr size_t MAX_BOOK_SESSIONS = 100;

struct BookDetail {
  BookAggregate agg;
  bool hidden = false;
  ReadingStyle style = ReadingStyle::None;
  std::vector<ProgressPoint> progress;   // per reading day, at most MAX_PROGRESS_POINTS + 1
  std::vector<BookSessionRow> sessions;  // newest first, at most MAX_BOOK_SESSIONS
  uint32_t lateSessions = 0;             // counted sessions with late-night reading
  uint32_t lateSec = 0;
};
// What loadBookDetail fills besides the totals and style.
enum class BookDetailLoad : uint8_t { Totals, Progress, Full };  // Full: progress and session rows
// One book's counted sessions (sessions.bin, the running one). False for an unknown book.
bool loadBookDetail(uint16_t index, BookDetail& out, BookDetailLoad load = BookDetailLoad::Full);

// Calendar sessions that started on fromDay or later, spread over the weekday x hour grid
// (GRID_CELLS values).
void loadHourGrid(int32_t fromDay, uint32_t* grid);

// Records over the counted sessions of books not hidden and days.bin, the running session included.
bool loadRecords(Records& out);
// Years with counted calendar reading, newest first, at most maxYears; this year alone when there
// is none and the clock is set. Returns the count.
int loadReadingYears(int* out, int maxYears);
bool loadYearReview(int year, YearReview& out);
// Speed of the calendar sessions: the 12 months up to this one (the last reading day's without
// a clock), parts of the day, font sizes.
bool loadSpeedTrends(SpeedTrends& out);

// What hooked me: one book's chapters from its raw log, the running session included (its
// buffered records are written first).
struct BookHooks {
  HookTotals totals;
  // Chapters with reading in book order; spine items of one TOC entry are merged.
  std::unique_ptr<ChapterStats[]> chapters;
  size_t count = 0;
  // Chapter i's TOC title starts at titleAt[i] in titles (NUL-terminated; empty without a title).
  std::string titles;
  std::vector<uint16_t> titleAt;
};
// Two passes over the book's log; TOC entries and titles from the book's spine/TOC cache when it
// is there.
bool loadBookHooks(uint16_t index, BookHooks& out);

}  // namespace stats
