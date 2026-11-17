#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "StatsFormat.h"

// Reading-statistics rules with no hardware or storage access (host-tested in
// test/reading_stats). The rules are described in docs/reading-stats.md.
namespace stats {

// --- Active time --------------------------------------------------------------------------
// A page dwell counts as active up to a cap derived from the book's typical reading rate:
// CAP_FACTOR x median seconds per 1000 characters x the page's characters, within
// [MIN_CAP_SEC, MAX_CAP_SEC]. Without enough samples for the book the global rate is used,
// without enough global samples a flat DEFAULT_CAP_SEC.
constexpr float CAP_FACTOR = 3.5f;
constexpr uint32_t MIN_CAP_SEC = 60;
constexpr uint32_t MAX_CAP_SEC = 30 * 60;
constexpr uint32_t DEFAULT_CAP_SEC = 5 * 60;
constexpr uint32_t MIN_MODEL_SAMPLES = 20;
// Learning samples: forward turns to the next page of the same chapter only.
constexpr uint32_t MIN_LEARN_DWELL_MS = 2000;
constexpr uint32_t MAX_LEARN_DWELL_MS = 30 * 60 * 1000;
constexpr uint16_t MIN_LEARN_CHARS = 200;
// Screen page size assumed before a book has a measured one.
constexpr uint16_t DEFAULT_PAGE_CHARS = 1000;
// Histogram buckets per doubling of the rate (s per 1000 chars); bucket 0 starts at 1 s.
constexpr int BUCKETS_PER_OCTAVE = 4;
// Raw records store this offset when the page's text offset is not known.
constexpr uint32_t UNKNOWN_OFFSET = 0xFFFFFFFF;
// A session ended by automatic sleep counts its last stretch at most as one typical page dwell
// (median rate x typical page length); this before any rate is known.
constexpr uint32_t IDLE_TRIM_DEFAULT_SEC = 60;
// One dictionary sheet counts at most this long (a sheet left open with sleep set to Never).
constexpr uint32_t DICT_MAX_SEC = 10 * 60;

// --- Sessions -----------------------------------------------------------------------------
// Short open: less active time than this, or no page turn. Ignored when the setting is on.
constexpr uint32_t SHORT_OPEN_SEC = 60;
// Sessions of one book less than this apart (end to start) form one sitting.
constexpr uint32_t SITTING_GAP_SEC = 10 * 60;
// Whole-book progress that counts as finished; the end-of-book screen reports 10000.
constexpr uint16_t FINISHED_BP = 9900;
constexpr uint16_t END_OF_BOOK_BP = 10000;

int rateBucket(float secPerKChar);
// Adds one sample; halves every bucket when one would overflow, so old reading fades.
void addRateSample(uint16_t* hist, uint32_t& samples, float secPerKChar);
// Adds a histogram to another, halving the target first when a bucket would overflow.
void mergeRateHist(uint16_t* into, uint32_t& intoSamples, const uint16_t* add, uint32_t addSamples);
// Median rate in s per 1000 chars, 0 for an empty histogram.
float medianRate(const uint16_t* hist);

// The rate a session uses for its caps: the book's when it has MIN_MODEL_SAMPLES, else the
// global one when that has, else 0 (flat cap).
float chooseRate(const uint16_t* bookHist, uint32_t bookSamples, const uint16_t* globalHist, uint32_t globalSamples);
uint32_t dwellCapSec(float rate, uint16_t pageChars);

// Where a page is in the book, as the reader shows it.
struct PagePos {
  uint16_t spine = 0;
  uint16_t page = 0;       // page in the spine item's layout
  uint16_t pageCount = 0;  // pages in that layout
  uint32_t offset = 0;     // visible text offset in the spine item
  bool offsetKnown = false;
};

// TurnKind from one shown page to the next; 0 when it is the same page (a repaint).
uint8_t classifyTurn(const PagePos& from, const PagePos& to);

// Active time of one session, fed with page changes in display order. The live recorder
// and the log replay drive the same clock, so both give the same numbers.
class ActiveClock {
 public:
  struct Turn {
    uint8_t kind = 0;
    bool charsKnown = false;  // next page in the same chapter: exact text length
    uint16_t chars = 0;
    uint32_t countedMs = 0;  // active part of the dwell that ended
    bool learned = false;    // a speed sample: rate in s per 1000 chars
    float rate = 0.0f;
  };

  void begin(float rate, uint16_t typicalPageChars);
  // First page of the session.
  void start(uint32_t nowMs, const PagePos& pos);
  // A page was shown. Kind 0 = same page: nothing changes, the dwell goes on.
  Turn pageShown(uint32_t nowMs, const PagePos& pos);
  // A page change of a known kind (log replay; pageShown after classifying).
  Turn turn(uint32_t nowMs, uint8_t kind, const PagePos& pos);
  // Time while paused (a menu over the page) is not part of any dwell. A page change
  // ends the pause.
  void pause(uint32_t nowMs);
  void resume(uint32_t nowMs);
  bool isPaused() const { return paused; }
  // Dictionary sheet over the page: its time is reading time, apart from the page dwell (no
  // page cap, no speed sample), at most DICT_MAX_SEC per sheet. Opening it ends a pause; a page
  // change or a pause closes it.
  void dictionaryOpen(uint32_t nowMs);
  void dictionaryClose(uint32_t nowMs);
  bool inDictionary() const { return inDict; }
  // Active ms including the open stretch (page dwell or dictionary) until nowMs. trimIdle:
  // the session ended by automatic sleep, so the open stretch counts at most typicalDwellMs();
  // not while paused (that dwell ended before the pause).
  uint32_t activeMsAt(uint32_t nowMs, bool trimIdle = false) const;
  // Dictionary ms until nowMs (part of activeMsAt).
  uint32_t dictionaryMsAt(uint32_t nowMs, bool trimIdle = false) const;
  // One page at the session's rate and typical page length; IDLE_TRIM_DEFAULT_SEC without a rate.
  uint32_t typicalDwellMs() const;
  void setRate(float newRate) { rate = newRate; }
  uint16_t typicalPageChars() const { return typicalChars; }
  bool hasPage() const { return started; }

 private:
  uint32_t capMs(uint16_t chars) const;
  // Counted part of the dwell on the page on screen (frozen while paused or in the dictionary).
  uint32_t openDwellMs(uint32_t nowMs) const;
  float rate = 0.0f;
  uint16_t typicalChars = DEFAULT_PAGE_CHARS;
  bool started = false;
  bool paused = false;
  bool inDict = false;
  PagePos last;
  uint32_t lastMs = 0;
  uint32_t pausedAtMs = 0;
  uint32_t dictSinceMs = 0;
  uint32_t activeMs = 0;  // closed page dwells
  uint32_t dictMs = 0;    // closed dictionary stretches
};

bool isShortOpen(uint32_t activeSec, uint32_t turns);
// Unknown wall time on either side never joins a sitting.
bool sameSitting(uint32_t prevEndUtc, uint32_t startUtc);

// The one rule for what counts: in totals (short-open rule), and in the calendar (also a
// known wall time and a book not hidden).
bool countsInTotals(const SessionRecord& session, bool ignoreShort);
bool hasWallTime(const SessionRecord& session);
bool countsInCalendar(const SessionRecord& session, bool ignoreShort, bool bookExcluded);
// The session took the book across FINISHED_BP or to the end-of-book screen (SESSION_FINISHED).
bool finishesBook(const SessionRecord& session);
bool crossesFinish(uint16_t startPercentBp, uint16_t percentBp);
// Paper pages moved forward (both positions known), as the day totals count them.
uint16_t paperPagesForward(const SessionRecord& session);

// --- Raw log ------------------------------------------------------------------------------
// Reads up to n bytes into buf; returns the count, less than n at the end of the data.
using ReadFn = size_t (*)(void* ctx, void* buf, size_t n);

// Frames raw-log records after the file header.
class LogScanner {
 public:
  LogScanner(ReadFn read, void* ctx, size_t startOffset) : read(read), ctx(ctx), end(startOffset) {}
  // The next complete record: its known prefix (at most RAW_MAX_KNOWN_SIZE bytes) into buf,
  // the rest skipped. False at the end of the data or at a torn record.
  bool next(uint8_t* buf, size_t& size);
  // Offset just past the last complete record: where a torn tail starts.
  size_t completeEnd() const { return end; }

 private:
  ReadFn read;
  void* ctx;
  size_t end;
};

// Rebuilds SessionRecords from one book's raw-log records.
class LogReplay {
 public:
  // skipSeq: a session number not to emit (the one still running), 0 for none. idleTrim:
  // trim automatic-sleep sessions written before the trim (needs a learned book rate).
  void begin(uint16_t bookIndex, float rate, uint16_t typicalPageChars, uint32_t skipSeq, bool idleTrim);
  // One framed record (size = bytes in record). True when it completed a session into out.
  bool feed(const uint8_t* record, size_t size, SessionRecord& out);
  // Closes a session the log left open; true when there was one.
  bool finish(SessionRecord& out);
  // The open session's clock and its last page change (per-chapter replays read them). The clock
  // is reset by the next start record.
  const ActiveClock& sessionClock() const { return clock; }
  const ActiveClock::Turn& lastTurn() const { return turnResult; }

 private:
  bool close(const RawEnd* end, SessionRecord& out);
  uint16_t bookIndex = 0;
  float rate = 0.0f;
  uint16_t typicalChars = 0;
  uint32_t skipSeq = 0;
  bool idleTrim = false;
  bool open = false;
  SessionRecord rec{};
  ActiveClock clock;
  ActiveClock::Turn turnResult;
  PagePos lastPos;
  uint32_t lastMs = 0;
};

// --- Calendar (local time) ----------------------------------------------------------------
int32_t daysFromCivil(int year, unsigned month, unsigned day);
void civilFromDays(int32_t days, int& year, unsigned& month, unsigned& day);
int32_t localDay(uint32_t utc, int16_t tzOffsetMin);
// Minutes east of UTC at utc, under the process time zone (TZ, set from the clock setting).
int16_t localTzOffsetMin(uint32_t utc);
// Monday = 0.
int weekday(int32_t day);
// Weeks start on Monday; week 0 holds 1970-01-01.
int32_t weekIndex(int32_t day);
int32_t weekStartDay(int32_t week);
// year * 12 + month - 1.
int32_t monthIndex(int32_t day);
int32_t yearOf(int32_t day);

// --- Summaries ----------------------------------------------------------------------------
struct ApplyResult {
  bool counted = false;     // countsInTotals
  bool newSitting = false;  // counted and not joined to the book's previous session
  bool calendar = false;    // countsInCalendar
  DayRecord day{};          // the day's share when calendar
};

// Adds a finalized session to its book's totals and says what it adds to the calendar.
ApplyResult applySession(BookTotals& totals, bool bookExcluded, const SessionRecord& session, bool ignoreShort);
// Adds one day's share to an ascending day list.
void addDay(std::vector<DayRecord>& days, const DayRecord& add);
void mergeDay(DayRecord& into, const DayRecord& add);

struct Streaks {
  int current = 0;  // ends today, or yesterday when today has no reading yet
  int longest = 0;
};
// Streaks over days fed one by one in ascending order (days without active time are skipped).
class StreakCounter {
 public:
  void add(const DayRecord& day);
  Streaks result(int32_t today) const;

 private:
  int run = 0;
  int longest = 0;
  int32_t prev = 0;
  bool hasPrev = false;
};
// days: ascending, each with active time.
Streaks computeStreaks(const DayRecord* days, size_t count, int32_t today);

// The Overview's values from the calendar days.
struct Overview {
  uint32_t todaySec = 0;  // today, this week (from Monday), month and year; 0 without a clock
  uint32_t weekSec = 0;
  uint32_t monthSec = 0;
  uint32_t yearSec = 0;
  uint32_t daysSec = 0;  // all days (Total adds the time without a date)
  int32_t anchor = 0;    // the day the charts end on: today, or without a clock the last day
  Streaks streaks;       // up to anchor
};
// Fed with the days in ascending order.
class OverviewAccumulator {
 public:
  OverviewAccumulator(int32_t today, bool clockKnown);
  void add(const DayRecord& day);
  Overview result() const;

 private:
  Overview sums;
  bool known;
  int32_t weekStart = 0;
  int32_t monthStart = 0;
  int32_t yearStart = 0;
  int32_t lastDay = 0;
  StreakCounter streaks;
};

enum class Range : uint8_t { Day, Week, Month, Year };
int32_t bucketOf(Range range, int32_t day);
// Active seconds per bucket for buckets [firstBucket, firstBucket + count).
void bucketActive(const DayRecord* days, size_t dayCount, Range range, int32_t firstBucket, int count, uint32_t* out);
// First and last local day of a bucket.
void bucketDays(Range range, int32_t bucket, int32_t& firstDay, int32_t& lastDay);

// --- Weekday x hour grid ------------------------------------------------------------------
constexpr int GRID_CELLS = 7 * 24;
// Calls fn with each local hour a session's wall time covers and that hour's even share of the
// active time. Sessions without wall time add nothing.
using HourShareFn = void (*)(void* ctx, int32_t day, int hour, uint32_t sec);
void spreadOverHours(const SessionRecord& session, HourShareFn fn, void* ctx);
// Spreads a session over the grid; cell = weekday (Monday = 0) x 24 + hour.
void addToHourGrid(uint32_t* grid, const SessionRecord& session);
// Shade of a cell: 0 none, 1-3 thirds of the busiest cell.
int gridShade(uint32_t value, uint32_t max);

// --- Per book -----------------------------------------------------------------------------
// One paper page, as PaperPages counts them.
constexpr uint32_t PAPER_PAGE_CHARS = 1800;

// Reading style rules (docs/reading-stats.md), checked in this order.
enum class ReadingStyle : uint8_t { None, Stalled, Binge, Snacking, Steady, Intermittent };
constexpr uint32_t STYLE_MIN_SESSIONS = 2;       // dated sessions before a style is given
constexpr int32_t STALLED_DAYS = 30;             // unfinished, not read for this long
constexpr uint32_t BINGE_DAY_SEC = 60 * 60;      // average per reading day
constexpr uint32_t SNACK_SESSION_SEC = 10 * 60;  // average session shorter than this
// Steady and Binge: reading on at least half of the days from the first to the last.

struct StyleAcc {
  int32_t firstDay = 0;
  int32_t lastDay = 0;
  uint32_t activeSec = 0;
  uint16_t sessions = 0;
  uint16_t days = 0;  // local days with reading
};
// Adds a counted session (the caller applies countsInTotals); undated sessions are skipped.
void addStyleSession(StyleAcc& acc, const SessionRecord& session);
// todayKnown false: the Stalled rule needs the date and is skipped.
ReadingStyle classifyStyle(const StyleAcc& acc, bool finished, bool todayKnown, int32_t today);

struct ProgressPoint {
  int32_t day;
  uint16_t percentBp;  // where the day's last session ended
};
// Points kept for the progress chart; past this every second one is dropped (the first and
// the last stay), so the vector never grows past MAX_PROGRESS_POINTS + 1.
constexpr size_t MAX_PROGRESS_POINTS = 128;

// Everything the per-book screens show, from the book's sessions in order.
struct BookAggregate {
  BookTotals totals{};  // as booktotals.bin
  StyleAcc style;
  uint32_t lookups = 0;
  uint32_t dictSec = 0;
  uint16_t firstPercentBp = 0;  // where the first counted session started
  bool hasFirst = false;
  uint16_t lastPaperPage = 0;    // latest known, 0 = unknown
  uint16_t paperPagesTotal = 0;  // latest known, 0 = unknown
  int32_t finishedDay = 0;       // local day of the finish, when hasFinishedDay
  bool hasFinishedDay = false;
};
// Adds one session of the book; returns countsInTotals. Points: the day's end position
// for the progress chart (dated counted sessions), may be null; bounded by MAX_PROGRESS_POINTS.
bool addBookSession(BookAggregate& agg, const SessionRecord& session, bool ignoreShort,
                    std::vector<ProgressPoint>* points);
// Paper pages per hour from the exact text read; 0 below 5 minutes of measured reading.
uint32_t pagesPerHour(uint32_t charsRead, uint32_t learnedMs);
// Active seconds left: from the paper pages left and the reading speed when both are known,
// else from the time spent per percent so far; 0 when finished or not known.
uint32_t secondsToFinish(const BookAggregate& agg);
// Local days from the first dated session to the finish, both included, each by its own
// offset; 0 when not finished or undated.
int daysToFinish(const BookAggregate& agg);
// Progress chart columns over [firstDay, lastDay]: the position at the end of each column's
// last reading day, carried forward; 0 before the first point. points ascending by day.
void progressColumns(const ProgressPoint* points, size_t count, int32_t firstDay, int32_t lastDay, int columns,
                     uint16_t* out);

}  // namespace stats
