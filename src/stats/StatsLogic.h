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
  // Active ms including the open dwell on the page on screen until nowMs.
  uint32_t activeMsAt(uint32_t nowMs) const;
  void setRate(float newRate) { rate = newRate; }
  uint16_t typicalPageChars() const { return typicalChars; }
  bool hasPage() const { return started; }

 private:
  uint32_t capMs(uint16_t chars) const;
  float rate = 0.0f;
  uint16_t typicalChars = DEFAULT_PAGE_CHARS;
  bool started = false;
  bool paused = false;
  PagePos last;
  uint32_t lastMs = 0;
  uint32_t pausedAtMs = 0;
  uint32_t activeMs = 0;
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
  // skipSeq: a session number not to emit (the one still running), 0 for none.
  void begin(uint16_t bookIndex, float rate, uint16_t typicalPageChars, uint32_t skipSeq);
  // One framed record (size = bytes in record). True when it completed a session into out.
  bool feed(const uint8_t* record, size_t size, SessionRecord& out);
  // Closes a session the log left open; true when there was one.
  bool finish(SessionRecord& out);

 private:
  bool close(const RawEnd* end, SessionRecord& out);
  uint16_t bookIndex = 0;
  float rate = 0.0f;
  uint16_t typicalChars = 0;
  uint32_t skipSeq = 0;
  bool open = false;
  SessionRecord rec{};
  ActiveClock clock;
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
// days: ascending, each with active time.
Streaks computeStreaks(const DayRecord* days, size_t count, int32_t today);

enum class Range : uint8_t { Day, Week, Month, Year };
int32_t bucketOf(Range range, int32_t day);
// Active seconds per bucket for buckets [firstBucket, firstBucket + count).
void bucketActive(const DayRecord* days, size_t dayCount, Range range, int32_t firstBucket, int count, uint32_t* out);

}  // namespace stats
