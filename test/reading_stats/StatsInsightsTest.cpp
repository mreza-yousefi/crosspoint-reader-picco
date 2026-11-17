#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "StatsInsights.h"

using namespace stats;

namespace {
int32_t day(int y, unsigned m, unsigned d) { return daysFromCivil(y, m, d); }

// UTC seconds of a local time on a day, for a session stored with tzOffsetMin.
uint32_t at(int32_t localDay, int hour, int minute, int16_t tzOffsetMin = 0) {
  return static_cast<uint32_t>(static_cast<int64_t>(localDay) * 86400 + hour * 3600 + minute * 60 -
                               tzOffsetMin * 60);
}

SessionRecord dated(uint16_t book, uint32_t startUtc, uint32_t durationSec, uint32_t activeSec,
                    int16_t tzOffsetMin = 0) {
  SessionRecord s{};
  s.bookIndex = book;
  s.startUtc = startUtc;
  s.tzOffsetMin = tzOffsetMin;
  s.durationSec = durationSec;
  s.activeSec = activeSec;
  s.flags = SESSION_WALL_KNOWN;
  return s;
}

template <typename T>
void put(std::vector<uint8_t>& log, const T& record) {
  const auto* p = reinterpret_cast<const uint8_t*>(&record);
  log.insert(log.end(), p, p + sizeof(T));
}

RawStart start(uint32_t seq, uint16_t spine, uint32_t offset) {
  RawStart r{};
  r.type = RAW_START;
  r.size = sizeof(RawStart);
  r.spine = spine;
  r.offset = offset;
  r.sessionSeq = seq;
  return r;
}

RawTurn turn(uint32_t tMs, uint8_t kind, uint16_t spine, uint32_t offset, uint16_t dwellDs = 0) {
  RawTurn r{};
  r.dwellDs = dwellDs;
  r.type = RAW_TURN;
  r.size = sizeof(RawTurn);
  r.kind = kind;
  r.spine = spine;
  r.tMs = tMs;
  r.offset = offset;
  return r;
}

RawEvent event(uint32_t tMs, uint8_t kind) {
  RawEvent r{};
  r.type = RAW_EVENT;
  r.size = sizeof(RawEvent);
  r.kind = kind;
  r.tMs = tMs;
  return r;
}

RawEnd end(uint32_t durationMs, uint32_t activeSec, uint16_t turnsFwd, uint8_t flags = 0) {
  RawEnd r{};
  r.type = RAW_END;
  r.size = sizeof(RawEnd);
  r.reason = END_EXIT;
  r.flags = flags;
  r.durationMs = durationMs;
  r.activeSec = activeSec;
  r.turnsFwd = turnsFwd;
  return r;
}

// Splits a log body (no file header) into its records.
std::vector<std::pair<const uint8_t*, size_t>> records(const std::vector<uint8_t>& log) {
  std::vector<std::pair<const uint8_t*, size_t>> out;
  for (size_t i = 0; i + 2 <= log.size(); i += log[i + 1]) out.emplace_back(log.data() + i, log[i + 1]);
  return out;
}

struct HookResult {
  std::vector<ChapterStats> chapters;
  HookTotals totals;
};

HookResult replayHooks(const std::vector<uint8_t>& log, bool ignoreShort, uint32_t skipSeq = 0,
                       const std::vector<int16_t>* toc = nullptr) {
  ChapterReplay replay;
  replay.beginCount(40.0f, 1000, skipSeq, false, ignoreShort);
  for (const auto& [record, size] : records(log)) replay.count(record, size);
  replay.finishCount();
  HookResult result;
  result.chapters.resize(replay.chapterCount());
  replay.beginChapters(result.chapters.data(), result.chapters.size(), toc ? toc->data() : nullptr);
  for (const auto& [record, size] : records(log)) replay.feed(record, size);
  replay.finish();
  result.totals = replay.totals();
  return result;
}

// Three sessions: a long one crossing from chapter 1 into 2, a short open, a re-read with no end.
std::vector<uint8_t> hookLog() {
  std::vector<uint8_t> log;
  put(log, start(1, 1, 0));
  put(log, turn(60000, TURN_NEXT, 1, 1000));
  put(log, turn(120000, TURN_NEXT, 1, 2000));
  put(log, turn(180000, TURN_NEXT, 2, 0));  // into chapter 2
  put(log, event(200000, EVENT_LOOKUP));
  put(log, event(210000, EVENT_PAUSE));
  put(log, event(270000, EVENT_RESUME));
  put(log, turn(300000, TURN_NEXT, 2, 1000));
  put(log, turn(360000, TURN_NEXT, 2, 2000));
  put(log, turn(420000, TURN_NEXT, 2, 3000));  // third page in: one more chapter
  put(log, end(450000, 390, 6));               // 30 s of the last page

  put(log, start(2, 2, 3000));
  put(log, end(5000, 5, 0, SESSION_SHORT));

  put(log, start(3, 2, 1000));
  put(log, turn(50000, TURN_NEXT, 2, 2000));  // text read before
  put(log, turn(80000, TURN_PREV, 2, 1000));
  return log;
}
}  // namespace

// --- Late nights, lookups per hour, parts of the day ---------------------------------------

TEST(ReadingStatsLateNight, CountsOnlyTheLateHours) {
  // 22:30-00:30 local (UTC+2), 2 h active: 23:00-00:30 is late.
  const int16_t tz = 120;
  const SessionRecord s = dated(0, at(day(2026, 3, 10), 22, 30, tz), 7200, 7200, tz);
  EXPECT_EQ(lateNightSec(s), 5400u);
  EXPECT_EQ(lateNightSec(dated(0, at(day(2026, 3, 10), 14, 0), 3600, 3000)), 0u);
  // 03:30-04:30: the half hour before LATE_END_HOUR.
  EXPECT_EQ(lateNightSec(dated(0, at(day(2026, 3, 10), 3, 30), 3600, 3600)), 1800u);
  SessionRecord undated = dated(0, 0, 3600, 3600);
  undated.flags = 0;
  EXPECT_EQ(lateNightSec(undated), 0u);
}

TEST(ReadingStatsLateNight, LookupsPerHourAndDayParts) {
  EXPECT_EQ(lookupsPerHourTenths(7, 7200), 35u);
  EXPECT_EQ(lookupsPerHourTenths(1, 0), 0u);
  EXPECT_EQ(lookupsPerHourTenths(0, 3600), 0u);
  EXPECT_EQ(dayPartOf(4 * 60 + 59), DAY_NIGHT);
  EXPECT_EQ(dayPartOf(5 * 60), DAY_MORNING);
  EXPECT_EQ(dayPartOf(12 * 60), DAY_AFTERNOON);
  EXPECT_EQ(dayPartOf(17 * 60), DAY_EVENING);
  EXPECT_EQ(dayPartOf(22 * 60), DAY_NIGHT);
  EXPECT_EQ(localMinuteOfDay(at(day(2026, 3, 10), 0, 15, -300), -300), 15);
}

TEST(ReadingStatsLateNight, BusiestAndGridTotals) {
  const uint32_t values[] = {0, 5, 9, 9, 2};
  EXPECT_EQ(busiestIndex(values, 5), 2);
  const uint32_t none[] = {0, 0};
  EXPECT_EQ(busiestIndex(none, 2), -1);
  uint32_t grid[GRID_CELLS] = {};
  grid[2 * 24 + 21] = 100;
  grid[5 * 24 + 21] = 50;
  grid[5 * 24 + 9] = 120;
  uint32_t weekdays[7];
  uint32_t hours[24];
  gridTotals(grid, weekdays, hours);
  EXPECT_EQ(busiestIndex(weekdays, 7), 5);
  EXPECT_EQ(busiestIndex(hours, 24), 21);
}

// --- Records --------------------------------------------------------------------------------

TEST(ReadingStatsRecords, SessionsSittingsAndTimesOfDay) {
  std::vector<RecordsBook> books(2);
  Records r;
  const int32_t d = day(2026, 5, 4);
  // Book 0: two sessions 5 min apart form one 70 min sitting.
  addRecordSession(r, books, dated(0, at(d, 20, 0), 2400, 2400));
  addRecordSession(r, books, dated(0, at(d, 20, 45), 1800, 1800));
  // Book 1: the longest single session, ending at 01:30.
  SessionRecord late = dated(1, at(d, 23, 0), 9000, 3000);
  late.lookups = 4;
  addRecordSession(r, books, late);
  // Book 1: starts at 05:10, the earliest morning; 03:00 the night before is not morning.
  addRecordSession(r, books, dated(1, at(d + 1, 5, 10), 600, 600));
  addRecordSession(r, books, dated(1, at(d + 3, 3, 0), 600, 600));

  EXPECT_EQ(r.longestSession.value, 3000u);
  EXPECT_EQ(r.longestSession.book, 1);
  EXPECT_EQ(r.longestSitting.value, 4200u);
  EXPECT_EQ(r.longestSitting.book, 0);
  EXPECT_EQ(r.longestSitting.utc, at(d, 20, 0));
  EXPECT_EQ(r.mostLookups.value, 4u);
  EXPECT_EQ(r.latestNight.value, 3u * 60 + 10);  // the 03:00 session ended at 03:10
  EXPECT_EQ(r.earliestMorning.value, 5u * 60 + 10);
  EXPECT_EQ(r.lookups, 4u);
  EXPECT_EQ(r.lateSessions, 2u);
  EXPECT_EQ(r.lateSec, 3000u + 600u);  // 23:00-01:30 and 03:00-03:10, all late
}

TEST(ReadingStatsRecords, DaysWeeksStreakAndFastestBook) {
  std::vector<RecordsBook> books(3);
  books[0].charsRead = 180000;  // 100 pages in 1 h
  books[0].learnedMs = 3600000;
  books[1].charsRead = 90000;  // faster, but measured only 10 min
  books[1].learnedMs = 600000;
  books[2].charsRead = 90000;  // 50 pages in 1 h
  books[2].learnedMs = 3600000;
  const int32_t monday = day(2026, 6, 1);
  const std::vector<DayRecord> days = {
      {monday, 600, 1, 1, 0, 0},      {monday + 1, 900, 1, 1, 0, 0}, {monday + 2, 300, 1, 1, 0, 0},
      {monday + 7, 2000, 1, 1, 0, 0}, {monday + 9, 100, 1, 1, 0, 0},
  };
  Records r;
  finishRecords(r, books, days.data(), days.size());
  EXPECT_EQ(r.mostDay, monday + 7);
  EXPECT_EQ(r.mostDaySec, 2000u);
  EXPECT_EQ(r.mostWeek, weekIndex(monday + 7));
  EXPECT_EQ(r.mostWeekSec, 2100u);
  EXPECT_EQ(r.longestStreak, 3);
  EXPECT_EQ(r.fastestBook.book, 0);
  EXPECT_EQ(r.fastestBook.value, 100u);
}

// --- Year in review -------------------------------------------------------------------------

TEST(ReadingStatsYear, OnlyTheYearsCalendarSessions) {
  std::vector<YearBook> books(3);
  YearReview y;
  y.year = 2026;
  const int32_t sat = day(2026, 3, 7);
  addYearSession(y, books, dated(0, at(day(2025, 12, 30), 10, 0), 3600, 3600), true, false);
  SessionRecord a = dated(0, at(sat, 21, 0), 3600, 3600);
  a.lookups = 3;
  addYearSession(y, books, a, true, false);
  addYearSession(y, books, dated(0, at(sat + 1, 23, 30), 1800, 1800), true, false);
  addYearSession(y, books, dated(1, at(day(2026, 4, 2), 9, 0), 7200, 7200), true, false);
  addYearSession(y, books, dated(2, at(day(2026, 4, 3), 9, 0), 9000, 9000), true, true);  // hidden
  SessionRecord shortOpen = dated(1, at(day(2026, 4, 4), 9, 0), 30, 30);
  shortOpen.flags |= SESSION_SHORT | SESSION_FINISHED;
  addYearSession(y, books, shortOpen, true, false);

  EXPECT_EQ(y.activeSec, 3600u + 1800u + 7200u);
  EXPECT_EQ(y.sessions, 3u);
  EXPECT_EQ(y.lookups, 3u);
  EXPECT_EQ(y.months[2], 5400u);
  EXPECT_EQ(y.months[3], 7200u);
  EXPECT_EQ(y.lateSessions, 1u);
  EXPECT_EQ(y.lateSec, 1800u);
  EXPECT_TRUE(books[1].finished);  // a short session still finishes the book

  const std::vector<DayRecord> days = {{day(2025, 12, 30), 3600, 1, 1, 0, 0},
                                       {day(2025, 12, 31), 600, 1, 1, 0, 0},
                                       {sat, 3600, 1, 1, 0, 0},
                                       {sat + 1, 1800, 1, 1, 0, 0},
                                       {day(2026, 4, 2), 7200, 1, 1, 0, 0}};
  finishYearReview(y, books, days.data(), days.size(), true, day(2026, 4, 5));
  EXPECT_EQ(y.longestStreak, 2);  // the 2025 days do not count
  ASSERT_EQ(y.topCount, 2);
  EXPECT_EQ(y.topBooks[0], 1);
  EXPECT_EQ(y.topSec[0], 7200u);
  EXPECT_EQ(y.topBooks[1], 0);
  EXPECT_EQ(y.styles[static_cast<int>(ReadingStyle::None)], 1);  // book 1: one counted session
  EXPECT_EQ(y.styles[static_cast<int>(ReadingStyle::Intermittent)], 1);  // book 0, from all its sessions
  uint32_t weekdays[7];
  uint32_t hours[24];
  gridTotals(y.grid, weekdays, hours);
  EXPECT_EQ(busiestIndex(weekdays, 7), 3);  // Thursday 2 April
  EXPECT_EQ(busiestIndex(hours, 24), 9);
}

TEST(ReadingStatsYear, TopBooksKeepTheLargest) {
  std::vector<YearBook> books(5);
  const uint32_t seconds[] = {100, 500, 300, 400, 200};
  for (int i = 0; i < 5; ++i) books[i].yearSec = seconds[i];
  YearReview y;
  y.year = 2026;
  finishYearReview(y, books, nullptr, 0, false, 0);
  ASSERT_EQ(y.topCount, YEAR_TOP_BOOKS);
  EXPECT_EQ(y.topBooks[0], 1);
  EXPECT_EQ(y.topBooks[1], 3);
  EXPECT_EQ(y.topBooks[2], 2);
}

// --- Speed trends ---------------------------------------------------------------------------

TEST(ReadingStatsSpeed, ByMonthPartOfDayAndFontSize) {
  SpeedTrends t;
  t.firstMonth = monthIndex(day(2026, 1, 1));
  SessionRecord a = dated(0, at(day(2026, 2, 3), 8, 0), 3600, 3600);
  a.charsRead = 90000;
  a.learnedMs = 1800000;
  a.fontSize = 14;
  SessionRecord b = dated(0, at(day(2026, 2, 9), 23, 0), 3600, 3600);
  b.charsRead = 60000;
  b.learnedMs = 1800000;
  b.fontSize = 10;
  SessionRecord old = dated(0, at(day(2025, 12, 1), 13, 0), 600, 600);
  old.charsRead = 10000;
  old.learnedMs = 600000;
  old.fontSize = 14;
  SessionRecord unmeasured = dated(0, at(day(2026, 2, 3), 13, 0), 600, 600);
  unmeasured.fontSize = 20;
  for (const auto& s : {a, b, old, unmeasured}) addSpeedSession(t, s);

  EXPECT_EQ(t.months[1].charsRead, 150000u);
  EXPECT_EQ(t.months[1].learnedMs, 3600000u);
  EXPECT_EQ(cellPagesPerHour(t.dayParts[DAY_MORNING]), 100u);
  EXPECT_EQ(t.dayParts[DAY_NIGHT].charsRead, 60000u);
  EXPECT_EQ(t.dayParts[DAY_AFTERNOON].charsRead, 10000u);  // outside the month window, still by time of day
  ASSERT_EQ(t.fontCount, 2);
  EXPECT_EQ(t.fontSizes[0], 10);
  EXPECT_EQ(t.fontSizes[1], 14);
  EXPECT_EQ(t.fonts[1].charsRead, 100000u);
}

// --- What hooked me -------------------------------------------------------------------------

TEST(ReadingStatsHooks, ChaptersFromTheRawLog) {
  const HookResult r = replayHooks(hookLog(), true);
  ASSERT_EQ(r.chapters.size(), 3u);
  const ChapterStats& one = r.chapters[1];
  const ChapterStats& two = r.chapters[2];
  EXPECT_EQ(r.chapters[0].activeMs, 0u);
  EXPECT_EQ(one.spine, 1);
  EXPECT_EQ(one.activeMs, 180000u);
  EXPECT_EQ(one.charsRead, 2000u);
  EXPECT_EQ(one.learnedMs, 120000u);
  EXPECT_EQ(one.starts, 1);
  EXPECT_EQ(one.ends, 0);
  // Chapter 2: 3 pages + the 30 s left of session 1, then 50 s + 30 s of session 3.
  EXPECT_EQ(two.activeMs, 210000u + 80000u);
  EXPECT_EQ(two.starts, 1);  // the short open is left out
  EXPECT_EQ(two.ends, 2);
  EXPECT_EQ(two.lookups, 1);
  EXPECT_EQ(two.pauses, 1);
  EXPECT_EQ(two.oneMore, 1);
  EXPECT_EQ(two.turnsBack, 1);
  EXPECT_EQ(two.rereadChars, 1000u);
  EXPECT_EQ(two.charsRead, 4000u);
  EXPECT_EQ(r.totals.sessions, 2u);
  EXPECT_EQ(r.totals.oneMoreSessions, 1u);
  EXPECT_TRUE(r.totals.hasLastEnd);
  EXPECT_EQ(r.totals.lastEndSpine, 2);
  // The chapters add up to the sessions' active time (390 s + 80 s).
  EXPECT_EQ(one.activeMs + two.activeMs, 470000u);
}

TEST(ReadingStatsHooks, ShortOpensCountWhenNotIgnoredAndSkippedSessionDoesNot) {
  const HookResult all = replayHooks(hookLog(), false);
  EXPECT_EQ(all.chapters[2].starts, 2);
  EXPECT_EQ(all.chapters[2].ends, 3);
  EXPECT_EQ(all.chapters[2].activeMs, 210000u + 5000u + 80000u);
  EXPECT_EQ(all.totals.sessions, 3u);

  const HookResult skipped = replayHooks(hookLog(), true, 3);  // session 3 still running
  EXPECT_EQ(skipped.chapters[2].ends, 1);
  EXPECT_EQ(skipped.chapters[2].rereadChars, 0u);
  EXPECT_EQ(skipped.totals.sessions, 1u);
}

TEST(ReadingStatsHooks, LateSpineItemsFoldIntoTheLastEntry) {
  std::vector<uint8_t> log;
  put(log, start(1, 0, 0));
  put(log, turn(100000, TURN_JUMP_FWD, 300, 0));
  put(log, end(200000, 200, 1));
  const HookResult r = replayHooks(log, true);
  ASSERT_EQ(r.chapters.size(), MAX_CHAPTERS);
  EXPECT_EQ(r.chapters[0].activeMs, 100000u);
  EXPECT_EQ(r.chapters[MAX_CHAPTERS - 1].activeMs, 100000u);
  EXPECT_EQ(r.chapters[MAX_CHAPTERS - 1].ends, 1);
}

TEST(ReadingStatsHooks, NoSessionsNoChapters) {
  ChapterReplay replay;
  replay.beginCount(0.0f, 0, 0, false, true);
  replay.finishCount();
  EXPECT_EQ(replay.chapterCount(), 0u);
}

TEST(ReadingStatsHooks, HighlightsAndMerge) {
  ChapterStats c[3] = {};
  c[0].activeMs = 600000;
  c[0].charsRead = 180000;  // 100 pages/h
  c[0].learnedMs = 3600000;
  c[1].activeMs = 900000;
  c[1].charsRead = 90000;  // 50 pages/h
  c[1].learnedMs = 3600000;
  c[1].ends = 3;
  c[2].turnsBack = 4;
  c[2].rereadChars = 500;
  const HookHighlights h = pickHighlights(c, 3);
  EXPECT_EQ(h.mostTime, 1);
  EXPECT_EQ(h.fastest, 0);
  EXPECT_EQ(h.slowest, 1);
  EXPECT_EQ(h.mostEnds, 1);
  EXPECT_EQ(h.mostBack, 2);
  EXPECT_EQ(h.mostReread, 2);
  EXPECT_EQ(h.mostPauses, -1);
  EXPECT_EQ(h.mostLookups, -1);

  c[0].spine = 4;
  mergeChapter(c[0], c[1]);
  EXPECT_EQ(c[0].activeMs, 1500000u);
  EXPECT_EQ(c[0].ends, 3);
  EXPECT_EQ(c[0].spine, 4);
}

TEST(ReadingStatsHooks, RecordedDwellsMakeChapterTimeExact) {
  std::vector<uint8_t> log;
  put(log, start(1, 1, 0));
  put(log, turn(60000, TURN_NEXT, 1, 1000, 500));  // 50 s counted by the recorder
  put(log, turn(120000, TURN_NEXT, 2, 0, 300));    // 30 s
  put(log, end(150000, 100, 2));                   // the last page: 100 - 80 = 20 s
  const HookResult r = replayHooks(log, true);
  EXPECT_EQ(r.chapters[1].activeMs, 80000u);
  EXPECT_EQ(r.chapters[2].activeMs, 20000u);
  EXPECT_EQ(r.chapters[1].learnedMs, 50000u);
}

TEST(ReadingStatsHooks, OlderLogsScaleOverTheSessionsChapters) {
  // Replayed: 60 s in chapter 1, 30 s open in chapter 2; the session counted 180 s.
  std::vector<uint8_t> log;
  put(log, start(1, 1, 0));
  put(log, turn(60000, TURN_NEXT, 2, 0));
  put(log, end(90000, 180, 1));
  const HookResult r = replayHooks(log, true);
  EXPECT_EQ(r.chapters[1].activeMs, 120000u);
  EXPECT_EQ(r.chapters[2].activeMs, 60000u);
}

TEST(ReadingStatsHooks, ManyChaptersInOneSessionStillAddUp) {
  std::vector<uint8_t> log;
  put(log, start(1, 0, 0));
  for (uint16_t spine = 1; spine < 20; ++spine) put(log, turn(spine * 10000u, TURN_NEXT, spine, 0));
  put(log, end(205000, 200, 19));
  const HookResult r = replayHooks(log, true);
  uint64_t sum = 0;
  for (const auto& c : r.chapters) sum += c.activeMs;
  EXPECT_EQ(sum, 200000u);
  EXPECT_GT(r.chapters[0].activeMs, 0u);
  EXPECT_EQ(r.chapters[19].activeMs, 0u);  // past MAX_SESSION_CHAPTERS: folded into the 16th
  EXPECT_EQ(r.chapters[19].ends, 1);
}

TEST(ReadingStatsHooks, OneMoreChapterFollowsTheToc) {
  // Spine 0 is the cover; spines 1 and 2 are one chapter split in two files; spine 3 the next.
  const std::vector<int16_t> toc = {-1, 0, 0, 1};
  std::vector<uint8_t> log;
  put(log, start(1, 0, 0));
  uint32_t t = 0;
  put(log, turn(t += 30000, TURN_NEXT, 1, 0));  // cover -> chapter: not "one more"
  for (uint32_t o = 1000; o <= 3000; o += 1000) put(log, turn(t += 30000, TURN_NEXT, 1, o));
  put(log, turn(t += 30000, TURN_NEXT, 2, 0));  // file split: not a new chapter
  for (uint32_t o = 1000; o <= 3000; o += 1000) put(log, turn(t += 30000, TURN_NEXT, 2, o));
  put(log, turn(t += 30000, TURN_NEXT, 3, 0));  // a new chapter
  for (uint32_t o = 1000; o <= 3000; o += 1000) put(log, turn(t += 30000, TURN_NEXT, 3, o));
  put(log, end(t + 10000, (t + 10000) / 1000, 13));
  const HookResult r = replayHooks(log, true, 0, &toc);
  EXPECT_EQ(r.chapters[1].oneMore, 0);
  EXPECT_EQ(r.chapters[2].oneMore, 0);
  EXPECT_EQ(r.chapters[3].oneMore, 1);
  EXPECT_EQ(r.totals.oneMoreSessions, 1u);
  // Without the TOC every spine item is a chapter.
  const HookResult bySpine = replayHooks(log, true);
  EXPECT_EQ(bySpine.chapters[1].oneMore + bySpine.chapters[2].oneMore + bySpine.chapters[3].oneMore, 3);
}

TEST(ReadingStatsRecords, UndatedSessionsAndNightsPastFour) {
  std::vector<RecordsBook> books(1);
  Records r;
  SessionRecord undated = dated(0, 0, 7200, 7000);
  undated.flags = 0;
  undated.lookups = 9;
  addRecordSession(r, books, undated);
  EXPECT_EQ(r.longestSession.value, 7000u);
  EXPECT_EQ(r.longestSession.utc, 0u);
  EXPECT_EQ(r.mostLookups.value, 9u);
  EXPECT_EQ(r.latestNight.book, NO_BOOK);  // needs a time of day
  EXPECT_EQ(r.longestSitting.book, NO_BOOK);
  // 02:30 to 05:00 reads past LATE_END_HOUR: the latest night is 03:59.
  addRecordSession(r, books, dated(0, at(day(2026, 7, 1), 2, 30), 9000, 3000));
  EXPECT_EQ(r.latestNight.value, 3u * 60 + 59);
  EXPECT_EQ(r.lookups, 9u);
  EXPECT_EQ(r.activeSec, 10000u);
}

TEST(ReadingStatsHooks, SessionWithoutEndIsNotScaled) {
  // The running session: no end record, so its recorded dwells stand as they are.
  std::vector<uint8_t> log;
  put(log, start(1, 1, 0));
  put(log, turn(60000, TURN_NEXT, 1, 1000, 500));
  put(log, turn(120000, TURN_NEXT, 2, 0, 300));
  const HookResult r = replayHooks(log, true);
  EXPECT_EQ(r.chapters[1].activeMs, 80000u);
  EXPECT_EQ(r.chapters[2].activeMs, 0u);
  EXPECT_EQ(r.chapters[2].ends, 1);
}

TEST(ReadingStatsHooks, DroppedRecordsAreEstimatedAndScaled) {
  std::vector<uint8_t> log;
  put(log, start(1, 1, 0));
  put(log, turn(60000, TURN_NEXT, 1, 1000, 500));
  put(log, turn(120000, TURN_NEXT, 2, 0, 300));
  put(log, end(150000, 100, 2, SESSION_RECORDS_DROPPED));
  const HookResult r = replayHooks(log, true);
  // 80 s recorded + 30 s estimated last page, scaled to 100 s.
  EXPECT_EQ(r.chapters[1].activeMs, 72728u);
  EXPECT_EQ(r.chapters[2].activeMs, 27272u);
}
