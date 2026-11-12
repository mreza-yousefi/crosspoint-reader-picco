#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <vector>

#include "StatsLogic.h"

using namespace stats;

namespace {
PagePos pos(uint16_t spine, uint16_t page, uint32_t offset, uint16_t pageCount = 50) {
  PagePos p;
  p.spine = spine;
  p.page = page;
  p.pageCount = pageCount;
  p.offset = offset;
  p.offsetKnown = true;
  return p;
}

SessionRecord session(uint32_t startUtc, uint32_t durationSec, uint32_t activeSec, uint8_t flags = SESSION_WALL_KNOWN) {
  SessionRecord s{};
  s.startUtc = startUtc;
  s.durationSec = durationSec;
  s.activeSec = activeSec;
  s.flags = flags;
  return s;
}

int32_t day(int y, unsigned m, unsigned d) { return daysFromCivil(y, m, d); }
}  // namespace

// --- Active time ------------------------------------------------------------------------

TEST(ReadingStatsActiveTime, FlatCapWithoutModel) {
  ActiveClock clock;
  clock.begin(0.0f, 0);
  clock.pageShown(0, pos(1, 0, 0));
  clock.pageShown(10 * 60 * 1000, pos(1, 1, 900));  // 10 min on one page
  EXPECT_EQ(clock.activeMsAt(10 * 60 * 1000), DEFAULT_CAP_SEC * 1000);
}

TEST(ReadingStatsActiveTime, CapScalesWithRateAndPageSize) {
  EXPECT_EQ(dwellCapSec(40.0f, 1000), 140u);            // 3.5 x 40 s per 1000 chars
  EXPECT_EQ(dwellCapSec(40.0f, 2000), 280u);            // twice the text, twice the cap
  EXPECT_EQ(dwellCapSec(5.0f, 500), MIN_CAP_SEC);       // floor
  EXPECT_EQ(dwellCapSec(900.0f, 2000), MAX_CAP_SEC);    // ceiling
  EXPECT_EQ(dwellCapSec(0.0f, 1000), DEFAULT_CAP_SEC);  // no model
}

TEST(ReadingStatsActiveTime, ClipsLongDwellToBookCap) {
  ActiveClock clock;
  clock.begin(40.0f, 1000);
  clock.pageShown(0, pos(2, 4, 10000));
  clock.pageShown(100000, pos(2, 5, 11000));   // 100 s, under the 140 s cap
  clock.pageShown(1100000, pos(2, 6, 12000));  // 1000 s away: clipped to 140 s
  EXPECT_EQ(clock.activeMsAt(1100000), 240000u);
}

TEST(ReadingStatsActiveTime, OpenDwellCountsUpToCap) {
  ActiveClock clock;
  clock.begin(40.0f, 1000);
  clock.pageShown(0, pos(0, 0, 0));
  EXPECT_EQ(clock.activeMsAt(30000), 30000u);
  EXPECT_EQ(clock.activeMsAt(3600000), 140000u);
}

TEST(ReadingStatsActiveTime, RepaintKeepsDwellRunning) {
  ActiveClock clock;
  clock.begin(40.0f, 1000);
  clock.pageShown(0, pos(0, 3, 500));
  EXPECT_EQ(clock.pageShown(20000, pos(0, 3, 500)).kind, 0);
  EXPECT_EQ(clock.pageShown(50000, pos(0, 4, 1500)).kind, TURN_NEXT);
  EXPECT_EQ(clock.activeMsAt(50000), 50000u);
}

TEST(ReadingStatsActiveTime, LearnsFromNextPageInChapter) {
  ActiveClock clock;
  clock.begin(0.0f, 0);
  clock.pageShown(0, pos(3, 0, 0));
  const auto turn = clock.pageShown(30000, pos(3, 1, 1000));
  ASSERT_TRUE(turn.learned);
  EXPECT_FLOAT_EQ(turn.rate, 30.0f);  // 30 s for 1000 chars
  EXPECT_EQ(turn.chars, 1000);
  // Back turns, jumps and too-short dwells teach nothing.
  EXPECT_FALSE(clock.pageShown(60000, pos(3, 0, 0)).learned);
  EXPECT_FALSE(clock.pageShown(60500, pos(3, 1, 1000)).learned);
  EXPECT_FALSE(clock.pageShown(90000, pos(7, 0, 0)).learned);
}

TEST(ReadingStatsActiveTime, PauseIsNotReading) {
  ActiveClock clock;
  clock.begin(40.0f, 1000);
  clock.pageShown(0, pos(1, 0, 0));
  clock.pause(30000);  // menu opened after 30 s on the page
  EXPECT_EQ(clock.activeMsAt(600000), 30000u);
  clock.resume(600000);  // ten minutes in the menu
  const auto turn = clock.pageShown(620000, pos(1, 1, 1000));
  EXPECT_EQ(turn.countedMs, 50000u);  // 30 s before, 20 s after the menu
  EXPECT_TRUE(turn.learned);
  EXPECT_FLOAT_EQ(turn.rate, 50.0f);
}

TEST(ReadingStatsActiveTime, PageChangeEndsPause) {
  ActiveClock clock;
  clock.begin(40.0f, 1000);
  clock.pageShown(0, pos(1, 0, 0));
  clock.pause(10000);
  EXPECT_EQ(clock.pageShown(50000, pos(1, 0, 0)).kind, 0);  // repaint under the menu
  EXPECT_TRUE(clock.isPaused());
  const auto turn = clock.pageShown(70000, pos(4, 0, 0));  // a chapter jump from the menu
  EXPECT_EQ(turn.kind, TURN_JUMP_FWD);
  EXPECT_EQ(turn.countedMs, 10000u);
  EXPECT_FALSE(clock.isPaused());
}

TEST(ReadingStatsActiveTime, CharsReadOnlyFromSameChapterNextPages) {
  ActiveClock clock;
  clock.begin(40.0f, 1000);
  clock.pageShown(0, pos(1, 0, 0));
  auto turn = clock.pageShown(30000, pos(1, 1, 900));
  EXPECT_TRUE(turn.charsKnown);
  EXPECT_EQ(turn.chars, 900);
  EXPECT_EQ(turn.countedMs, 30000u);
  turn = clock.pageShown(60000, pos(2, 0, 0));  // next chapter: length of the last page unknown
  EXPECT_FALSE(turn.charsKnown);
  PagePos unknown = pos(2, 1, 0);
  unknown.offsetKnown = false;
  EXPECT_FALSE(clock.pageShown(90000, unknown).charsKnown);
}

TEST(ReadingStatsActiveTime, ModelFallsBackToGlobalThenFlat) {
  uint16_t book[RATE_BUCKETS] = {};
  uint16_t global[RATE_BUCKETS] = {};
  uint32_t bookSamples = 0;
  uint32_t globalSamples = 0;
  EXPECT_EQ(chooseRate(book, bookSamples, global, globalSamples), 0.0f);
  for (int i = 0; i < 25; ++i) addRateSample(global, globalSamples, 60.0f);
  const float globalRate = chooseRate(book, bookSamples, global, globalSamples);
  EXPECT_NEAR(globalRate, 60.0f, 60.0f * 0.2f);
  for (int i = 0; i < 25; ++i) addRateSample(book, bookSamples, 20.0f);
  EXPECT_NEAR(chooseRate(book, bookSamples, global, globalSamples), 20.0f, 20.0f * 0.2f);
}

TEST(ReadingStatsActiveTime, MedianIgnoresOutliers) {
  uint16_t hist[RATE_BUCKETS] = {};
  uint32_t samples = 0;
  for (int i = 0; i < 30; ++i) addRateSample(hist, samples, 40.0f);
  for (int i = 0; i < 10; ++i) addRateSample(hist, samples, 900.0f);
  EXPECT_NEAR(medianRate(hist), 40.0f, 40.0f * 0.2f);
}

TEST(ReadingStatsActiveTime, HistogramHalvesOnOverflow) {
  uint16_t hist[RATE_BUCKETS] = {};
  uint32_t samples = 0;
  hist[rateBucket(40.0f)] = 0xFFFF;
  hist[rateBucket(80.0f)] = 100;
  samples = 0xFFFF + 100;
  addRateSample(hist, samples, 40.0f);
  EXPECT_EQ(hist[rateBucket(40.0f)], 0x7FFF + 1);
  EXPECT_EQ(hist[rateBucket(80.0f)], 50);
}

// --- Turns --------------------------------------------------------------------------------

TEST(ReadingStatsTurns, Classify) {
  EXPECT_EQ(classifyTurn(pos(2, 5, 900), pos(2, 5, 900)), 0);
  EXPECT_EQ(classifyTurn(pos(2, 5, 900), pos(2, 6, 1800)), TURN_NEXT);
  EXPECT_EQ(classifyTurn(pos(2, 5, 900), pos(2, 4, 0)), TURN_PREV);
  EXPECT_EQ(classifyTurn(pos(2, 49, 9000), pos(3, 0, 0)), TURN_NEXT);
  EXPECT_EQ(classifyTurn(pos(3, 0, 0), pos(2, 19, 9000, 20)), TURN_PREV);
  EXPECT_EQ(classifyTurn(pos(3, 0, 0), pos(2, 0, 0, 20)), TURN_JUMP_BACK);
  EXPECT_EQ(classifyTurn(pos(2, 5, 900), pos(2, 15, 9000)), TURN_JUMP_FWD);
  EXPECT_EQ(classifyTurn(pos(2, 5, 900), pos(9, 0, 0)), TURN_JUMP_FWD);
  // Same page index after a reflow: the text offset decides.
  EXPECT_EQ(classifyTurn(pos(2, 5, 900), pos(2, 5, 700)), TURN_JUMP_BACK);
}

// --- Sessions -----------------------------------------------------------------------------

TEST(ReadingStatsSessions, ShortOpen) {
  EXPECT_TRUE(isShortOpen(59, 4));
  EXPECT_TRUE(isShortOpen(600, 0));
  EXPECT_FALSE(isShortOpen(60, 1));
}

TEST(ReadingStatsSessions, SittingMergesCloseSessions) {
  EXPECT_TRUE(sameSitting(10000, 10000 + SITTING_GAP_SEC - 1));
  EXPECT_FALSE(sameSitting(10000, 10000 + SITTING_GAP_SEC));
  EXPECT_TRUE(sameSitting(10000, 9990));  // overlap from clock jitter
  EXPECT_FALSE(sameSitting(0, 10000));
  EXPECT_FALSE(sameSitting(10000, 0));
}

TEST(ReadingStatsSessions, ApplyCountsSittingsAndCalendar) {
  BookTotals totals{};
  const uint32_t t0 = 1767225600;  // 2026-01-01 00:00 UTC
  ApplyResult r = applySession(totals, false, session(t0, 1200, 900), true);
  EXPECT_TRUE(r.counted);
  EXPECT_TRUE(r.newSitting);
  EXPECT_TRUE(r.calendar);
  EXPECT_EQ(r.day.day, day(2026, 1, 1));
  // Re-opened five minutes after closing: same sitting.
  r = applySession(totals, false, session(t0 + 1200 + 300, 600, 500), true);
  EXPECT_FALSE(r.newSitting);
  EXPECT_EQ(r.day.sittings, 0);
  // An hour later: a new sitting.
  r = applySession(totals, false, session(t0 + 6000, 600, 400), true);
  EXPECT_TRUE(r.newSitting);
  EXPECT_EQ(totals.sessions, 3u);
  EXPECT_EQ(totals.sittings, 2u);
  EXPECT_EQ(totals.totalActiveSec, 1800u);
  EXPECT_EQ(totals.firstUtc, t0);
}

TEST(ReadingStatsSessions, ShortOpenFilter) {
  BookTotals totals{};
  const SessionRecord shortOpen = session(1767225600, 40, 30, SESSION_WALL_KNOWN | SESSION_SHORT);
  EXPECT_FALSE(applySession(totals, false, shortOpen, true).counted);
  EXPECT_EQ(totals.sessions, 0u);
  EXPECT_TRUE(applySession(totals, false, shortOpen, false).counted);
  EXPECT_EQ(totals.totalActiveSec, 30u);
}

TEST(ReadingStatsSessions, ExcludedBookKeepsTotalsButNotCalendar) {
  BookTotals totals{};
  const ApplyResult r = applySession(totals, true, session(1767225600, 600, 500), true);
  EXPECT_TRUE(r.counted);
  EXPECT_FALSE(r.calendar);
  EXPECT_EQ(totals.totalActiveSec, 500u);
}

TEST(ReadingStatsSessions, UnknownWallTimeCountsInTotalsOnly) {
  BookTotals totals{};
  const ApplyResult r = applySession(totals, false, session(0, 600, 500, 0), true);
  EXPECT_TRUE(r.counted);
  EXPECT_FALSE(r.calendar);
  EXPECT_TRUE(r.newSitting);
  EXPECT_EQ(totals.firstUtc, 0u);
}

TEST(ReadingStatsSessions, FinishedAndPagesRead) {
  BookTotals totals{};
  SessionRecord s = session(1767225600, 600, 500);
  s.startPaperPage = 200;
  s.endPaperPage = 215;
  s.startPercentBp = 9500;
  s.endPercentBp = END_OF_BOOK_BP;  // the end-of-book screen
  s.flags |= SESSION_FINISHED;
  s.charsRead = 12000;
  s.learnedMs = 400000;
  const ApplyResult r = applySession(totals, false, s, true);
  EXPECT_EQ(r.day.pagesRead, 15);
  EXPECT_EQ(totals.finishedUtc, 1767225600u + 600u);
  EXPECT_EQ(totals.maxPercentBp, END_OF_BOOK_BP);
  EXPECT_EQ(totals.charsRead, 12000u);
  EXPECT_EQ(totals.learnedMs, 400000u);
}

TEST(ReadingStatsSessions, FinishedOnlyByFlag) {
  BookTotals totals{};
  SessionRecord below = session(1767225600, 600, 500);
  below.startPercentBp = 9800;
  below.endPercentBp = 9850;
  applySession(totals, false, below, true);
  EXPECT_EQ(totals.finishedUtc, 0u);
  EXPECT_TRUE(crossesFinish(9800, 9900));
  EXPECT_FALSE(crossesFinish(9950, END_OF_BOOK_BP));  // opened at the end again
}

TEST(ReadingStatsSessions, ShortFinalSessionStillFinishes) {
  BookTotals totals{};
  SessionRecord last = session(1767225600, 40, 30, SESSION_WALL_KNOWN | SESSION_SHORT | SESSION_FINISHED);
  last.startPercentBp = 9890;
  last.endPercentBp = 9910;
  const ApplyResult r = applySession(totals, false, last, true);
  EXPECT_FALSE(r.counted);
  EXPECT_EQ(totals.sessions, 0u);
  EXPECT_EQ(totals.finishedUtc, 1767225600u + 40u);
}

// --- Summaries ----------------------------------------------------------------------------

TEST(ReadingStatsSummaries, AddDayKeepsOrderAndMerges) {
  std::vector<DayRecord> days;
  addDay(days, DayRecord{10, 100, 1, 1, 0, 0});
  addDay(days, DayRecord{12, 50, 1, 1, 0, 0});
  addDay(days, DayRecord{11, 70, 1, 0, 0, 0});
  addDay(days, DayRecord{12, 25, 1, 0, 3, 0});
  ASSERT_EQ(days.size(), 3u);
  EXPECT_EQ(days[0].day, 10);
  EXPECT_EQ(days[1].day, 11);
  EXPECT_EQ(days[2].day, 12);
  EXPECT_EQ(days[2].activeSec, 75u);
  EXPECT_EQ(days[2].sessions, 2);
  EXPECT_EQ(days[2].sittings, 1);
  EXPECT_EQ(days[2].pagesRead, 3);
}

TEST(ReadingStatsSummaries, Streaks) {
  const std::vector<DayRecord> days = {{100, 60, 1, 1, 0, 0},
                                       {101, 60, 1, 1, 0, 0},
                                       {102, 60, 1, 1, 0, 0},
                                       {105, 60, 1, 1, 0, 0},
                                       {106, 60, 1, 1, 0, 0}};
  Streaks s = computeStreaks(days.data(), days.size(), 106);
  EXPECT_EQ(s.current, 2);
  EXPECT_EQ(s.longest, 3);
  s = computeStreaks(days.data(), days.size(), 107);  // not read yet today
  EXPECT_EQ(s.current, 2);
  s = computeStreaks(days.data(), days.size(), 108);
  EXPECT_EQ(s.current, 0);
  EXPECT_EQ(computeStreaks(nullptr, 0, 5).longest, 0);
}

// --- Calendar -----------------------------------------------------------------------------

TEST(ReadingStatsCalendar, CivilRoundTrip) {
  EXPECT_EQ(day(1970, 1, 1), 0);
  for (int32_t d = day(2024, 1, 1); d < day(2029, 1, 1); ++d) {
    int y;
    unsigned m, dd;
    civilFromDays(d, y, m, dd);
    ASSERT_EQ(day(y, m, dd), d);
  }
}

TEST(ReadingStatsCalendar, WeeksStartOnMonday) {
  EXPECT_EQ(weekday(0), 3);                  // 1970-01-01 was a Thursday
  EXPECT_EQ(weekday(day(2026, 11, 9)), 0);   // Monday
  EXPECT_EQ(weekday(day(2026, 11, 15)), 6);  // Sunday
  const int32_t week = weekIndex(day(2026, 11, 11));
  EXPECT_EQ(weekStartDay(week), day(2026, 11, 9));
  EXPECT_EQ(weekIndex(day(2026, 11, 15)), week);
  EXPECT_EQ(weekIndex(day(2026, 11, 16)), week + 1);
  EXPECT_EQ(weekIndex(day(1969, 12, 29)), 0);  // negative days stay on the grid
  EXPECT_EQ(weekIndex(day(1969, 12, 28)), -1);
}

TEST(ReadingStatsCalendar, LocalDayUsesOffset) {
  const uint32_t lateUtc = 1767225600 - 30 * 60;  // 2025-12-31 23:30 UTC
  EXPECT_EQ(localDay(lateUtc, 0), day(2025, 12, 31));
  EXPECT_EQ(localDay(lateUtc, 60), day(2026, 1, 1));                // UTC+1
  EXPECT_EQ(localDay(1767225600 + 3600, -300), day(2025, 12, 31));  // UTC-5
}

TEST(ReadingStatsCalendar, MonthAndYearBuckets) {
  EXPECT_EQ(monthIndex(day(2026, 1, 31)), 2026 * 12);
  EXPECT_EQ(monthIndex(day(2026, 2, 1)), 2026 * 12 + 1);
  EXPECT_EQ(yearOf(day(2026, 12, 31)), 2026);

  const std::vector<DayRecord> days = {{day(2026, 1, 5), 100, 1, 1, 0, 0},
                                       {day(2026, 1, 31), 50, 1, 1, 0, 0},
                                       {day(2026, 2, 2), 30, 1, 1, 0, 0},
                                       {day(2026, 4, 1), 7, 1, 1, 0, 0}};
  uint32_t months[3] = {};
  bucketActive(days.data(), days.size(), Range::Month, monthIndex(day(2026, 1, 1)), 3, months);
  EXPECT_EQ(months[0], 150u);
  EXPECT_EQ(months[1], 30u);
  EXPECT_EQ(months[2], 0u);

  uint32_t weeks[2] = {};
  bucketActive(days.data(), days.size(), Range::Week, weekIndex(day(2026, 1, 26)), 2, weeks);
  EXPECT_EQ(weeks[0], 50u);  // Mon 26 Jan - Sun 1 Feb
  EXPECT_EQ(weeks[1], 30u);  // Mon 2 Feb

  uint32_t years[1] = {};
  bucketActive(days.data(), days.size(), Range::Year, 2026, 1, years);
  EXPECT_EQ(years[0], 187u);
}

// --- Raw log ------------------------------------------------------------------------------

namespace {
struct Buffer {
  std::vector<uint8_t> bytes;
  size_t pos = 0;
};

size_t readBuffer(void* ctx, void* out, size_t n) {
  auto& b = *static_cast<Buffer*>(ctx);
  const size_t take = std::min(n, b.bytes.size() - b.pos);
  memcpy(out, b.bytes.data() + b.pos, take);
  b.pos += take;
  return take;
}

template <typename T>
void put(std::vector<uint8_t>& log, const T& record) {
  const auto* p = reinterpret_cast<const uint8_t*>(&record);
  log.insert(log.end(), p, p + sizeof(T));
}

RawStart rawStart(uint32_t seq, uint32_t startUtc, uint16_t spine, uint32_t offset) {
  RawStart r{};
  r.type = RAW_START;
  r.size = sizeof(RawStart);
  r.flags = startUtc ? SESSION_WALL_KNOWN : 0;
  r.startUtc = startUtc;
  r.spine = spine;
  r.offset = offset;
  r.sessionSeq = seq;
  return r;
}

RawTurn rawTurn(uint32_t tMs, uint8_t kind, uint16_t spine, uint32_t offset) {
  RawTurn r{};
  r.type = RAW_TURN;
  r.size = sizeof(RawTurn);
  r.kind = kind;
  r.spine = spine;
  r.tMs = tMs;
  r.offset = offset;
  return r;
}

RawEnd rawEnd(uint32_t durationMs, uint32_t activeSec, uint32_t endUtc) {
  RawEnd r{};
  r.type = RAW_END;
  r.size = sizeof(RawEnd);
  r.reason = END_EXIT;
  r.durationMs = durationMs;
  r.activeSec = activeSec;
  r.endUtc = endUtc;
  r.turnsFwd = 2;
  return r;
}

RawEvent rawEvent(uint32_t tMs, uint8_t kind) {
  RawEvent r{};
  r.type = RAW_EVENT;
  r.size = sizeof(RawEvent);
  r.kind = kind;
  r.tMs = tMs;
  return r;
}

std::vector<SessionRecord> replayAll(std::vector<uint8_t> bytes, size_t* completeEnd = nullptr, uint32_t skip = 0) {
  Buffer buffer{std::move(bytes), sizeof(FileHeader)};
  LogScanner scanner(&readBuffer, &buffer, sizeof(FileHeader));
  LogReplay replay;
  replay.begin(7, 40.0f, 1000, skip);
  std::vector<SessionRecord> out;
  uint8_t buf[RAW_MAX_KNOWN_SIZE];
  size_t size = 0;
  SessionRecord rec{};
  while (scanner.next(buf, size)) {
    if (replay.feed(buf, size, rec)) out.push_back(rec);
  }
  if (replay.finish(rec)) out.push_back(rec);
  if (completeEnd) *completeEnd = scanner.completeEnd();
  return out;
}

std::vector<uint8_t> emptyLog() { return std::vector<uint8_t>(sizeof(FileHeader), 0); }
}  // namespace

TEST(ReadingStatsRawLog, ReplayMatchesLiveClock) {
  auto log = emptyLog();
  put(log, rawStart(1, 0, 1, 0));
  put(log, rawTurn(100000, TURN_NEXT, 1, 1000));
  put(log, rawTurn(900000, TURN_NEXT, 1, 2000));
  const auto sessions = replayAll(log);  // no end record: closed at the last record
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions[0].activeSec, 240u);  // 100 s + 800 s clipped to 140 s
  EXPECT_EQ(sessions[0].charsRead, 2000u);
  EXPECT_EQ(sessions[0].learnedMs, 240000u);
  EXPECT_EQ(sessions[0].bookIndex, 7);
  EXPECT_TRUE(sessions[0].flags & SESSION_CRASHED);

  ActiveClock live;
  live.begin(40.0f, 1000);
  live.pageShown(0, pos(1, 0, 0));
  live.pageShown(100000, pos(1, 1, 1000));
  live.pageShown(900000, pos(1, 2, 2000));
  EXPECT_EQ(live.activeMsAt(900000) / 1000, sessions[0].activeSec);
}

TEST(ReadingStatsRawLog, EndRecordDatesSessionStartedWithoutClock) {
  auto log = emptyLog();
  put(log, rawStart(3, 0, 0, 0));
  put(log, rawTurn(30000, TURN_NEXT, 0, 900));
  RawEnd end = rawEnd(600000, 300, 1767226200);
  end.tzOffsetMin = 60;
  put(log, end);
  const auto sessions = replayAll(log);
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions[0].startUtc, 1767226200u - 600u);
  EXPECT_EQ(sessions[0].tzOffsetMin, 60);
  EXPECT_TRUE(hasWallTime(sessions[0]));
  EXPECT_EQ(sessions[0].activeSec, 300u);
}

TEST(ReadingStatsRawLog, SkipsUnknownTypesAndLongerRecords) {
  auto log = emptyLog();
  put(log, rawStart(1, 1767225600, 0, 0));
  const uint8_t future[6] = {'Z', 6, 1, 2, 3, 4};  // a type from a later version
  log.insert(log.end(), future, future + sizeof(future));
  RawTurn longer = rawTurn(30000, TURN_NEXT, 0, 900);
  longer.size = sizeof(RawTurn) + 4;  // a later version appended a field
  put(log, longer);
  const uint8_t extra[4] = {9, 9, 9, 9};
  log.insert(log.end(), extra, extra + sizeof(extra));
  put(log, rawEnd(60000, 60, 1767225660));
  size_t completeEnd = 0;
  const auto sessions = replayAll(log, &completeEnd);
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions[0].turnsFwd, 2);  // from the end record
  EXPECT_EQ(completeEnd, log.size());
}

TEST(ReadingStatsRawLog, TornTailCutThenLaterSessionsReplay) {
  auto log = emptyLog();
  put(log, rawStart(1, 1767225600, 0, 0));
  put(log, rawTurn(30000, TURN_NEXT, 0, 900));
  const size_t goodEnd = log.size();
  const RawTurn torn = rawTurn(60000, TURN_NEXT, 0, 1800);
  const auto* tornBytes = reinterpret_cast<const uint8_t*>(&torn);
  log.insert(log.end(), tornBytes, tornBytes + 9);

  // Recovery: find the last complete record, cut there, close the session.
  size_t completeEnd = 0;
  replayAll(log, &completeEnd);
  ASSERT_EQ(completeEnd, goodEnd);
  log.resize(completeEnd);
  RawEnd crash = rawEnd(30000, 30, 1767225630);
  crash.reason = END_CRASH;
  crash.flags = SESSION_WALL_KNOWN | SESSION_CRASHED;
  put(log, crash);
  // A later session of the same book.
  put(log, rawStart(2, 1767230000, 0, 900));
  put(log, rawTurn(40000, TURN_NEXT, 0, 1800));
  put(log, rawEnd(80000, 80, 1767230080));

  const auto sessions = replayAll(log);
  ASSERT_EQ(sessions.size(), 2u);
  EXPECT_EQ(sessions[0].sessionSeq, 1u);
  EXPECT_EQ(sessions[0].endReason, END_CRASH);
  EXPECT_EQ(sessions[1].sessionSeq, 2u);
  EXPECT_EQ(sessions[1].startUtc, 1767230000u);
  EXPECT_EQ(sessions[1].activeSec, 80u);

  // Without the cut, the torn bytes would misframe every later record.
  std::vector<uint8_t> uncut(log.begin(), log.begin() + goodEnd);
  uncut.insert(uncut.end(), tornBytes, tornBytes + 9);
  uncut.insert(uncut.end(), log.begin() + goodEnd, log.end());
  EXPECT_NE(replayAll(uncut).size(), 2u);
}

TEST(ReadingStatsRawLog, PauseAndEndOfBookEventsReplay) {
  auto log = emptyLog();
  put(log, rawStart(1, 1767225600, 1, 0));
  put(log, rawEvent(30000, EVENT_PAUSE));
  put(log, rawEvent(600000, EVENT_RESUME));
  put(log, rawTurn(620000, TURN_NEXT, 1, 1000));
  put(log, rawEvent(625000, EVENT_END_OF_BOOK));
  const auto sessions = replayAll(log);
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions[0].learnedMs, 50000u);  // the ten minutes in the menu do not count
  EXPECT_EQ(sessions[0].endPercentBp, END_OF_BOOK_BP);
}

TEST(ReadingStatsRawLog, SkipsRunningSession) {
  auto log = emptyLog();
  put(log, rawStart(5, 1767225600, 0, 0));
  put(log, rawTurn(30000, TURN_NEXT, 0, 900));
  EXPECT_TRUE(replayAll(log, nullptr, 5).empty());
  EXPECT_EQ(replayAll(log).size(), 1u);
}

TEST(ReadingStatsRawLog, OlderShorterRecordReadsWithZeroedNewFields) {
  // A reader built after a field was appended to X still reads a version 1 record.
  struct LaterEnd {
    RawEnd v1;
    uint32_t addedLater;
  };
  RawEnd v1 = rawEnd(60000, 60, 1767225660);
  v1.charsRead = 1234;
  LaterEnd later{};
  later.addedLater = 0xFFFFFFFF;
  ASSERT_TRUE(readRawRecord(reinterpret_cast<const uint8_t*>(&v1), sizeof(v1), &later, sizeof(later)));
  EXPECT_EQ(later.v1.charsRead, 1234u);
  EXPECT_EQ(later.v1.activeSec, 60u);
  EXPECT_EQ(later.addedLater, 0u);
  // Below the version 1 size: damaged, not read.
  EXPECT_FALSE(readRawRecord(reinterpret_cast<const uint8_t*>(&v1), sizeof(v1) - 4, &later, sizeof(later)));
  const uint8_t unknown[4] = {'Z', 4, 0, 0};
  EXPECT_FALSE(readRawRecord(unknown, sizeof(unknown), &later, sizeof(later)));
}

TEST(ReadingStatsRawLog, DamagedShortRecordIsSkipped) {
  auto log = emptyLog();
  put(log, rawStart(1, 1767225600, 0, 0));
  RawTurn shortTurn = rawTurn(30000, TURN_NEXT, 0, 900);
  shortTurn.size = 12;  // below the version 1 size of T
  log.insert(log.end(), reinterpret_cast<const uint8_t*>(&shortTurn),
             reinterpret_cast<const uint8_t*>(&shortTurn) + 12);
  put(log, rawTurn(60000, TURN_NEXT, 0, 1800));
  const auto sessions = replayAll(log);
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions[0].turnsFwd, 1);
}

TEST(ReadingStatsRawLog, CrossingFinishSetsFlag) {
  auto log = emptyLog();
  RawStart start = rawStart(1, 1767225600, 30, 0);
  start.percentBp = 9800;
  put(log, start);
  RawTurn turn = rawTurn(30000, TURN_NEXT, 30, 900);
  turn.percentBp = 9950;
  put(log, turn);
  const auto sessions = replayAll(log);
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_TRUE(finishesBook(sessions[0]));
}

TEST(ReadingStatsRawLog, TurnBackAfterEndScreenKeepsFinished) {
  auto log = emptyLog();
  RawStart start = rawStart(1, 1767225600, 30, 0);
  start.percentBp = 9950;  // already past 99%: only the end screen finishes
  put(log, start);
  put(log, rawEvent(20000, EVENT_END_OF_BOOK));
  RawTurn back = rawTurn(25000, TURN_PREV, 30, 0);
  back.percentBp = 9950;
  put(log, back);
  const auto sessions = replayAll(log);
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_TRUE(finishesBook(sessions[0]));
  EXPECT_EQ(sessions[0].endPercentBp, 9950);
  BookTotals totals{};
  applySession(totals, false, sessions[0], true);
  EXPECT_NE(totals.finishedUtc, 0u);
}
