#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "StatsFormat.h"
#include "StatsLogic.h"

// Stage 3 views of the reading statistics: records, year in review, speed trends, late nights,
// lookups per hour and the per-chapter replay of a book's raw log. No hardware or storage
// access (host-tested in test/reading_stats); the rules are in docs/reading-stats.md.
namespace stats {

// --- Time of day ----------------------------------------------------------------------------
// Late night: local hours from LATE_START_HOUR to LATE_END_HOUR (23:00-04:00). The latest-night
// and earliest-morning records count the clock from LATE_END_HOUR, so 01:00 is later than 23:00.
constexpr int LATE_START_HOUR = 23;
constexpr int LATE_END_HOUR = 4;

// Local minute of the day (0-1439) of a UTC time.
int localMinuteOfDay(uint32_t utc, int16_t tzOffsetMin);
// Part of a session's active time inside the late-night hours, spread over the hours as in
// addToHourGrid; 0 without wall time.
uint32_t lateNightSec(const SessionRecord& session);
// Lookups per hour of active reading, in tenths; 0 without reading time.
uint32_t lookupsPerHourTenths(uint32_t lookups, uint32_t activeSec);

// Parts of the day by the session's local start time.
enum DayPart : uint8_t { DAY_MORNING = 0, DAY_AFTERNOON, DAY_EVENING, DAY_NIGHT, DAY_PART_COUNT };
constexpr int MORNING_START_HOUR = 5;
constexpr int AFTERNOON_START_HOUR = 12;
constexpr int EVENING_START_HOUR = 17;
constexpr int NIGHT_START_HOUR = 22;
DayPart dayPartOf(int minuteOfDay);

// Index of the largest value, -1 when all are 0.
int busiestIndex(const uint32_t* values, int count);
// Weekday (7) and hour (24) totals of a weekday x hour grid.
void gridTotals(const uint32_t* grid, uint32_t* weekdays, uint32_t* hours);

// --- Records --------------------------------------------------------------------------------
// The fastest book needs this much measured reading (charsRead / learnedMs).
constexpr uint32_t FASTEST_MIN_MEASURED_MS = 30 * 60 * 1000;

struct RecordRef {
  uint32_t value = 0;
  uint32_t utc = 0;  // start of the session or sitting; 0 = undated
  int16_t tzOffsetMin = 0;
  uint16_t book = NO_BOOK;  // NO_BOOK: no record
};

// Per book while the records are added (24 bytes).
struct RecordsBook {
  uint32_t lastEndUtc = 0;
  uint32_t sittingUtc = 0;
  uint32_t sittingSec = 0;
  uint32_t charsRead = 0;
  uint32_t learnedMs = 0;
  int16_t sittingTz = 0;
};

struct Records {
  RecordRef longestSession;   // active seconds
  RecordRef longestSitting;   // active seconds of the sitting
  RecordRef mostLookups;      // lookups in one session
  RecordRef latestNight;      // value: local minute of the day the session ended (at most 03:59)
  RecordRef earliestMorning;  // value: local minute of the day the session started
  RecordRef fastestBook;      // value: paper pages per hour
  int32_t mostDay = 0;        // local day
  uint32_t mostDaySec = 0;
  int32_t mostWeek = 0;  // weekIndex
  uint32_t mostWeekSec = 0;
  int longestStreak = 0;
  // Lookups per hour (all counted sessions) and late nights (dated ones).
  uint32_t lookups = 0;
  uint32_t activeSec = 0;
  uint32_t lateSessions = 0;
  uint32_t lateSec = 0;
};

// Adds a counted session of a book not hidden, in file order; books has one entry per book
// index. Longest session, most lookups and the fastest book take undated sessions too; the
// records with a day or time of day only dated ones.
void addRecordSession(Records& records, std::vector<RecordsBook>& books, const SessionRecord& session);
// The day and week records, the longest streak and the fastest book. days: ascending.
void finishRecords(Records& records, const std::vector<RecordsBook>& books, const DayRecord* days, size_t dayCount);

// --- Year in review -------------------------------------------------------------------------
constexpr int YEAR_TOP_BOOKS = 3;
constexpr int STYLE_COUNT = static_cast<int>(ReadingStyle::Intermittent) + 1;

// Per book while a year is added (24 bytes).
struct YearBook {
  StyleAcc style;  // all counted sessions, any year
  uint32_t yearSec = 0;
  bool finished = false;
};

struct YearReview {
  int year = 0;
  uint32_t activeSec = 0;
  uint32_t sessions = 0;
  uint32_t lookups = 0;
  uint32_t lateSessions = 0;
  uint32_t lateSec = 0;
  uint32_t months[12] = {};
  uint32_t grid[GRID_CELLS] = {};
  int longestStreak = 0;
  uint16_t styles[STYLE_COUNT] = {};  // books read in the year, by their style today
  uint16_t topBooks[YEAR_TOP_BOOKS] = {};
  uint32_t topSec[YEAR_TOP_BOOKS] = {};
  int topCount = 0;
};

// Adds any session of sessions.bin (forEachSession order); the year's time counts calendar
// sessions that started in review.year.
void addYearSession(YearReview& review, std::vector<YearBook>& books, const SessionRecord& session, bool ignoreShort,
                    bool hidden);
// Top books, styles and the year's longest streak. days: ascending calendar days.
void finishYearReview(YearReview& review, const std::vector<YearBook>& books, const DayRecord* days, size_t dayCount,
                      bool todayKnown, int32_t today);

// --- Speed trends ---------------------------------------------------------------------------
constexpr int SPEED_MONTHS = 12;
// Font sizes kept; sessions at further sizes are left out of that view.
constexpr int MAX_FONT_SIZES = 12;

struct SpeedCell {
  uint64_t charsRead = 0;
  uint64_t learnedMs = 0;
};
// pagesPerHour of a cell.
uint32_t cellPagesPerHour(const SpeedCell& cell);

struct SpeedTrends {
  int32_t firstMonth = 0;  // monthIndex of months[0]; the last is firstMonth + SPEED_MONTHS - 1
  SpeedCell months[SPEED_MONTHS];
  SpeedCell dayParts[DAY_PART_COUNT];
  uint8_t fontSizes[MAX_FONT_SIZES] = {};  // ascending
  SpeedCell fonts[MAX_FONT_SIZES];
  int fontCount = 0;
};

// Adds a calendar session's measured reading (charsRead, learnedMs).
void addSpeedSession(SpeedTrends& trends, const SessionRecord& session);

// --- What hooked me: per chapter from the raw log -------------------------------------------
// Spine items counted per book; later ones add to the last entry. 36 bytes each, so at most
// 9 KB while the screen is open.
constexpr size_t MAX_CHAPTERS = 256;
// Sessions per book whose short-open state is remembered between the two passes; later ones count.
constexpr size_t MAX_HOOK_SESSIONS = 8192;
// "One more chapter": next pages turned in a chapter a session entered by reading on.
constexpr uint16_t ONE_MORE_PAGES = 3;

struct ChapterStats {
  uint32_t activeMs;
  uint32_t charsRead;  // next pages in the chapter with an exact length (speed)
  uint32_t learnedMs;
  uint32_t maxOffset;    // furthest text offset reached by next pages
  uint32_t rereadChars;  // text of next pages behind maxOffset: read again
  uint16_t starts;       // sessions that started here
  uint16_t ends;         // sessions that ended here
  uint16_t pauses;       // pause events (a menu over the page)
  uint16_t turnsBack;    // previous pages and jumps back from here
  uint16_t oneMore;      // sessions that entered by reading on and read ONE_MORE_PAGES more
  uint16_t lookups;
  uint16_t spine;  // first spine item of the entry
};

struct HookTotals {
  uint32_t sessions = 0;
  uint32_t oneMoreSessions = 0;  // sessions with at least one "one more chapter"
  uint16_t lastEndSpine = 0;     // where the last counted session ended
  bool hasLastEnd = false;
};

// Adds b to a (merging spine items of one chapter); keeps a.spine.
void mergeChapter(ChapterStats& a, const ChapterStats& b);

// Chapters one session's time is spread over; more fold into the last one.
constexpr size_t MAX_SESSION_CHAPTERS = 16;

// Replays one book's raw log twice. Pass 1 (count) finds which sessions count in the totals
// and the highest spine item; pass 2 (feed) adds the counted sessions to per-chapter counters.
// Page time comes from the turn records (dwellDs) or, in older logs, from LogReplay's clock; each
// session's chapter times are then scaled to add up to its active time.
class ChapterReplay {
 public:
  void beginCount(float rate, uint16_t typicalPageChars, uint32_t skipSeq, bool idleTrim, bool ignoreShort);
  void count(const uint8_t* record, size_t size);
  void finishCount();
  // Entries pass 2 needs: highest spine item + 1, at most MAX_CHAPTERS; 0 without sessions.
  size_t chapterCount() const;

  // chapters: chapterCount() entries, zeroed by this call. tocOfSpine: each entry's TOC index
  // (-1 = none), or null to treat each spine item as a chapter for "one more chapter".
  void beginChapters(ChapterStats* chapters, size_t count, const int16_t* tocOfSpine);
  void feed(const uint8_t* record, size_t size);
  void finish();
  const HookTotals& totals() const { return hookTotals; }

 private:
  struct Share {
    uint16_t chapter;
    uint32_t ms;
  };

  bool isCounted(size_t ordinal) const;
  uint16_t chapterIndex(uint16_t spine) const;
  int chapterKey(uint16_t spine) const;
  void addShare(uint16_t spine, uint32_t ms);
  // The open stretch until endMs by the clock: the last page and its dictionary time.
  uint32_t openStretchMs(uint32_t endMs, bool trimIdle) const;
  // hasEnd: closed by an end record, whose active time the chapters are scaled to.
  void closeSession(const SessionRecord* closed, uint32_t openMs, bool hasEnd);

  LogReplay replay;
  float rate = 0.0f;
  uint16_t typicalChars = 0;
  uint32_t skipSeq = 0;
  bool idleTrim = false;
  bool ignoreShort = false;
  uint8_t countedBits[MAX_HOOK_SESSIONS / 8] = {};
  size_t sessions = 0;  // valid start records seen in this pass
  uint16_t maxSpine = 0;

  ChapterStats* chapters = nullptr;
  size_t chapterTotal = 0;
  const int16_t* toc = nullptr;
  HookTotals hookTotals;
  bool open = false;
  bool counted = false;
  bool exact = false;  // every page dwell of the session came from its turn record, none dropped
  PagePos pos;
  uint32_t lastMs = 0;
  uint32_t clockAtTurnMs = 0;  // the clock's active time right after the last page change
  uint32_t dictAtTurnMs = 0;   // its dictionary time then
  Share shares[MAX_SESSION_CHAPTERS] = {};
  size_t shareCount = 0;
  uint16_t enteredSpine = 0;  // "one more chapter" candidate
  int enteredKey = -1;
  uint16_t pagesInto = 0;
  bool armed = false;
  bool sessionOneMore = false;
};

// Rows of the Highlights view: index of the chapter with the most of each, -1 for none.
struct HookHighlights {
  int mostTime = -1;
  int fastest = -1;  // pagesPerHour, with a measured speed
  int slowest = -1;
  int mostStarts = -1;
  int mostEnds = -1;
  int mostReread = -1;
  int mostBack = -1;
  int mostPauses = -1;
  int mostLookups = -1;
};
HookHighlights pickHighlights(const ChapterStats* chapters, size_t count);

}  // namespace stats
