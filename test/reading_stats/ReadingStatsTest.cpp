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

std::vector<SessionRecord> replayAll(std::vector<uint8_t> bytes, size_t* completeEnd = nullptr, uint32_t skip = 0,
                                     bool idleTrim = true) {
  Buffer buffer{std::move(bytes), sizeof(FileHeader)};
  LogScanner scanner(&readBuffer, &buffer, sizeof(FileHeader));
  LogReplay replay;
  replay.begin(7, 40.0f, 1000, skip, idleTrim);
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

// --- Dictionary time ----------------------------------------------------------------------

TEST(ReadingStatsDictionary, CountsInFullOutsideThePageDwell) {
  ActiveClock clock;
  clock.begin(40.0f, 1000);  // page cap 140 s
  clock.pageShown(0, pos(1, 0, 0));
  clock.dictionaryOpen(20000);
  EXPECT_TRUE(clock.inDictionary());
  // Ten minutes in the definition: more than the page cap, all of it counts.
  EXPECT_EQ(clock.dictionaryMsAt(620000), 600000u);
  EXPECT_EQ(clock.activeMsAt(620000), 620000u);
  clock.dictionaryClose(620000);
  const auto turn = clock.pageShown(650000, pos(1, 1, 1000));
  EXPECT_EQ(turn.countedMs, 50000u);  // the page dwell without the lookup
  EXPECT_TRUE(turn.learned);
  EXPECT_FLOAT_EQ(turn.rate, 50.0f);  // the lookup taught nothing
  EXPECT_EQ(clock.activeMsAt(650000), 650000u);
  EXPECT_EQ(clock.dictionaryMsAt(650000), 600000u);
}

TEST(ReadingStatsDictionary, OpeningEndsPauseAndPageChangeCloses) {
  ActiveClock clock;
  clock.begin(40.0f, 1000);
  clock.pageShown(0, pos(1, 0, 0));
  clock.pause(10000);           // reader menu
  clock.dictionaryOpen(70000);  // Look Up from the menu
  EXPECT_FALSE(clock.isPaused());
  clock.pageShown(100000, pos(1, 1, 1000));  // closes the sheet
  EXPECT_FALSE(clock.inDictionary());
  EXPECT_EQ(clock.dictionaryMsAt(100000), 30000u);
  EXPECT_EQ(clock.activeMsAt(100000), 40000u);  // 10 s on the page, 30 s in the dictionary
}

TEST(ReadingStatsDictionary, OneSheetCountsAtMostTenMinutes) {
  ActiveClock clock;
  clock.begin(40.0f, 1000);
  clock.pageShown(0, pos(1, 0, 0));
  clock.dictionaryOpen(10000);
  // Left open for an hour (sleep set to Never).
  EXPECT_EQ(clock.dictionaryMsAt(10000 + 3600000), DICT_MAX_SEC * 1000);
  clock.dictionaryClose(10000 + 3600000);
  EXPECT_EQ(clock.dictionaryMsAt(10000 + 3600000), DICT_MAX_SEC * 1000);
  const auto turn = clock.pageShown(10000 + 3600000 + 20000, pos(1, 1, 1000));
  EXPECT_EQ(turn.countedMs, 30000u);  // the page dwell around the sheet
}

TEST(ReadingStatsDictionary, ReplayedFromEvents) {
  auto log = emptyLog();
  put(log, rawStart(1, 1767225600, 1, 0));
  put(log, rawEvent(20000, EVENT_DICT_OPEN));
  put(log, rawEvent(320000, EVENT_DICT_CLOSE));
  put(log, rawTurn(350000, TURN_NEXT, 1, 1000));
  const auto sessions = replayAll(log);
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions[0].dictSec, 300u);
  EXPECT_EQ(sessions[0].activeSec, 350u);
  EXPECT_EQ(sessions[0].learnedMs, 50000u);
}

TEST(ReadingStatsDictionary, EndRecordCarriesDictionaryTime) {
  auto log = emptyLog();
  put(log, rawStart(1, 1767225600, 1, 0));
  RawEnd end = rawEnd(400000, 350, 1767226000);
  end.dictSec = 300;
  put(log, end);
  const auto sessions = replayAll(log);
  ASSERT_EQ(sessions.size(), 1u);
  EXPECT_EQ(sessions[0].dictSec, 300u);
}

// --- Automatic-sleep idle trim ------------------------------------------------------------

TEST(ReadingStatsIdleTrim, LastDwellCountsOneTypicalPage) {
  ActiveClock clock;
  clock.begin(40.0f, 1000);  // typical page: 40 s, cap 140 s
  clock.pageShown(0, pos(1, 0, 0));
  clock.pageShown(30000, pos(1, 1, 1000));
  // Put down for ten minutes until automatic sleep.
  EXPECT_EQ(clock.activeMsAt(630000), 30000u + 140000u);
  EXPECT_EQ(clock.activeMsAt(630000, true), 30000u + 40000u);
  EXPECT_EQ(clock.typicalDwellMs(), 40000u);
  // A short last dwell is not raised.
  EXPECT_EQ(clock.activeMsAt(50000, true), 50000u);
}

TEST(ReadingStatsIdleTrim, NotWhilePaused) {
  ActiveClock clock;
  clock.begin(40.0f, 1000);
  clock.pageShown(0, pos(1, 0, 0));
  clock.pause(100000);  // read 100 s, then the menu; slept from the menu
  EXPECT_EQ(clock.activeMsAt(900000, true), 100000u);
  EXPECT_EQ(clock.activeMsAt(900000, true), clock.activeMsAt(900000));
}

TEST(ReadingStatsIdleTrim, FallbackWithoutRateAndDictionaryStretch) {
  ActiveClock clock;
  clock.begin(0.0f, 0);
  EXPECT_EQ(clock.typicalDwellMs(), IDLE_TRIM_DEFAULT_SEC * 1000);
  clock.pageShown(0, pos(1, 0, 0));
  clock.dictionaryOpen(20000);
  // Slept with the definition up: the dictionary stretch is the one trimmed.
  EXPECT_EQ(clock.dictionaryMsAt(920000, true), 60000u);
  EXPECT_EQ(clock.activeMsAt(920000, true), 20000u + 60000u);
}

TEST(ReadingStatsIdleTrim, RebuildTrimsOldAutoSleepSessionsOnce) {
  auto log = emptyLog();
  put(log, rawStart(1, 1767225600, 1, 0));
  put(log, rawTurn(30000, TURN_NEXT, 1, 1000));
  RawEnd end = rawEnd(630000, 170, 1767226230);  // written untrimmed: 30 s + 140 s cap
  end.reason = END_AUTO_SLEEP;
  put(log, end);
  // The same session written by a recorder that already trimmed it.
  put(log, rawStart(2, 1767230000, 1, 1000));
  put(log, rawTurn(30000, TURN_NEXT, 1, 2000));
  RawEnd trimmed = rawEnd(630000, 70, 1767230630);
  trimmed.reason = END_AUTO_SLEEP;
  trimmed.flags = SESSION_WALL_KNOWN | SESSION_IDLE_TRIMMED;
  put(log, trimmed);
  // Manual sleep keeps the 3.5x cap.
  put(log, rawStart(3, 1767240000, 1, 2000));
  put(log, rawTurn(30000, TURN_NEXT, 1, 3000));
  RawEnd manual = rawEnd(630000, 170, 1767240630);
  manual.reason = END_SLEEP;
  put(log, manual);

  // A book without a learned rate keeps the written time (the trim would be a guess).
  const auto untrimmed = replayAll(log, nullptr, 0, false);
  ASSERT_EQ(untrimmed.size(), 3u);
  EXPECT_EQ(untrimmed[0].activeSec, 170u);
  EXPECT_FALSE(untrimmed[0].flags & SESSION_IDLE_TRIMMED);

  const auto sessions = replayAll(log);
  ASSERT_EQ(sessions.size(), 3u);
  EXPECT_EQ(sessions[0].activeSec, 70u);
  EXPECT_TRUE(sessions[0].flags & SESSION_IDLE_TRIMMED);
  EXPECT_EQ(sessions[1].activeSec, 70u);
  EXPECT_EQ(sessions[2].activeSec, 170u);
  EXPECT_FALSE(sessions[2].flags & SESSION_IDLE_TRIMMED);
}

// --- Reading style ------------------------------------------------------------------------

namespace {
constexpr int32_t JAN1 = 20454;  // 2026-01-01

// Counted sessions of activeMin minutes at 20:00 UTC on the given days after 2026-01-01.
StyleAcc styleOf(const std::vector<int>& dayOffsets, uint32_t activeMin) {
  StyleAcc acc;
  for (const int d : dayOffsets) {
    addStyleSession(
        acc, session(1767225600 + static_cast<uint32_t>(d) * 86400 + 20 * 3600, activeMin * 60 + 60, activeMin * 60));
  }
  return acc;
}
}  // namespace

TEST(ReadingStatsStyle, Rules) {
  ASSERT_EQ(day(2026, 1, 1), JAN1);
  // Too little to say.
  EXPECT_EQ(classifyStyle(styleOf({0}, 90), false, true, JAN1), ReadingStyle::None);
  // Two long evenings in a row.
  EXPECT_EQ(classifyStyle(styleOf({0, 0, 1}, 50), false, true, JAN1 + 2), ReadingStyle::Binge);
  // Twenty minutes most days.
  EXPECT_EQ(classifyStyle(styleOf({0, 1, 2, 4, 5, 6}, 20), false, true, JAN1 + 6), ReadingStyle::Steady);
  // Five minutes at a time.
  EXPECT_EQ(classifyStyle(styleOf({0, 0, 0, 1, 1, 3}, 5), false, true, JAN1 + 3), ReadingStyle::Snacking);
  // Half an hour now and then.
  EXPECT_EQ(classifyStyle(styleOf({0, 7, 15, 24}, 30), false, true, JAN1 + 25), ReadingStyle::Intermittent);
  // Not opened for over a month, not finished.
  const StyleAcc old = styleOf({0, 1, 2}, 30);
  EXPECT_EQ(classifyStyle(old, false, true, JAN1 + 2 + STALLED_DAYS + 1), ReadingStyle::Stalled);
  EXPECT_EQ(classifyStyle(old, false, true, JAN1 + 2 + STALLED_DAYS), ReadingStyle::Steady);
  EXPECT_EQ(classifyStyle(old, true, true, JAN1 + 200), ReadingStyle::Steady);    // finished
  EXPECT_EQ(classifyStyle(old, false, false, JAN1 + 200), ReadingStyle::Steady);  // no clock
}

TEST(ReadingStatsStyle, UndatedSessionsDoNotCount) {
  StyleAcc acc;
  addStyleSession(acc, session(0, 600, 500, 0));
  EXPECT_EQ(acc.sessions, 0);
  addStyleSession(acc, session(1767225600, 600, 500));
  addStyleSession(acc, session(1767225600 + 3600, 600, 500));
  EXPECT_EQ(acc.sessions, 2);
  EXPECT_EQ(acc.days, 1);
  EXPECT_EQ(acc.activeSec, 1000u);
}

// --- Weekday x hour grid ------------------------------------------------------------------

TEST(ReadingStatsGrid, SpreadsOverHoursInLocalTime) {
  uint32_t grid[GRID_CELLS] = {};
  // Thu 2026-01-01 20:30 UTC, UTC+1: 21:30-22:30 local, 40 min active.
  SessionRecord s = session(1767225600 + 20 * 3600 + 1800, 3600, 2400);
  s.tzOffsetMin = 60;
  addToHourGrid(grid, s);
  const int thursday = weekday(day(2026, 1, 1));
  EXPECT_EQ(thursday, 3);
  EXPECT_EQ(grid[thursday * 24 + 21], 1200u);
  EXPECT_EQ(grid[thursday * 24 + 22], 1200u);
  uint32_t sum = 0;
  for (const uint32_t v : grid) sum += v;
  EXPECT_EQ(sum, 2400u);
}

TEST(ReadingStatsGrid, CrossesMidnightIntoNextWeekday) {
  uint32_t grid[GRID_CELLS] = {};
  // Sun 2026-01-04 23:45 UTC for 30 min: 15 min Sunday 23h, 15 min Monday 0h.
  addToHourGrid(grid, session(static_cast<uint32_t>(day(2026, 1, 4)) * 86400 + 23 * 3600 + 2700, 1800, 1800));
  EXPECT_EQ(grid[6 * 24 + 23], 900u);
  EXPECT_EQ(grid[0 * 24 + 0], 900u);
  // Undated sessions are left out.
  uint32_t empty[GRID_CELLS] = {};
  addToHourGrid(empty, session(0, 600, 600, 0));
  for (const uint32_t v : empty) EXPECT_EQ(v, 0u);
}

TEST(ReadingStatsGrid, Shades) {
  EXPECT_EQ(gridShade(0, 100), 0);
  EXPECT_EQ(gridShade(1, 100), 1);
  EXPECT_EQ(gridShade(33, 100), 1);
  EXPECT_EQ(gridShade(34, 100), 2);
  EXPECT_EQ(gridShade(67, 100), 3);
  EXPECT_EQ(gridShade(100, 100), 3);
  EXPECT_EQ(gridShade(5, 0), 0);
}

TEST(ReadingStatsCalendar, BucketDays) {
  int32_t first = 0;
  int32_t last = 0;
  bucketDays(Range::Week, weekIndex(day(2026, 11, 11)), first, last);
  EXPECT_EQ(first, day(2026, 11, 9));
  EXPECT_EQ(last, day(2026, 11, 15));
  bucketDays(Range::Month, monthIndex(day(2026, 2, 10)), first, last);
  EXPECT_EQ(first, day(2026, 2, 1));
  EXPECT_EQ(last, day(2026, 2, 28));
  bucketDays(Range::Month, monthIndex(day(2026, 12, 10)), first, last);
  EXPECT_EQ(last, day(2026, 12, 31));
  bucketDays(Range::Year, 2028, first, last);
  EXPECT_EQ(last - first + 1, 366);
}

// --- Per book -----------------------------------------------------------------------------

TEST(ReadingStatsBook, AggregatesOneBook) {
  BookAggregate agg;
  std::vector<ProgressPoint> points;
  SessionRecord a = session(1767225600, 1800, 1500);
  a.startPercentBp = 1000;
  a.endPercentBp = 1500;
  a.lookups = 2;
  a.dictSec = 90;
  a.endPaperPage = 40;
  SessionRecord b = session(1767225600 + 4000, 1200, 1000);
  b.startPercentBp = 1500;
  b.endPercentBp = 1800;
  b.lookups = 1;
  SessionRecord shortOpen = session(1767225600 + 90000, 30, 20, SESSION_WALL_KNOWN | SESSION_SHORT);
  shortOpen.endPercentBp = 1800;
  SessionRecord c = session(1767225600 + 2 * 86400, 1800, 1500);
  c.startPercentBp = 1800;
  c.endPercentBp = 2500;
  c.endPaperPage = 60;
  EXPECT_TRUE(addBookSession(agg, a, true, &points));
  EXPECT_TRUE(addBookSession(agg, b, true, &points));
  EXPECT_FALSE(addBookSession(agg, shortOpen, true, &points));
  EXPECT_TRUE(addBookSession(agg, c, true, &points));
  EXPECT_EQ(agg.totals.sessions, 3u);
  EXPECT_EQ(agg.totals.sittings, 3u);
  EXPECT_EQ(agg.totals.totalActiveSec, 4000u);
  EXPECT_EQ(agg.lookups, 3u);
  EXPECT_EQ(agg.dictSec, 90u);
  EXPECT_EQ(agg.firstPercentBp, 1000);
  EXPECT_EQ(agg.lastPaperPage, 60);
  EXPECT_EQ(agg.style.days, 2);
  ASSERT_EQ(points.size(), 2u);
  EXPECT_EQ(points[0].day, day(2026, 1, 1));
  EXPECT_EQ(points[0].percentBp, 1800);  // the day's last session
  EXPECT_EQ(points[1].percentBp, 2500);
}

TEST(ReadingStatsBook, SpeedAndTimeToFinish) {
  EXPECT_EQ(pagesPerHour(90000, 60 * 60 * 1000), 50u);  // 90,000 chars in an hour
  EXPECT_EQ(pagesPerHour(90000, 4 * 60 * 1000), 0u);    // too little measured
  BookAggregate agg;
  agg.totals.charsRead = 90000;
  agg.totals.learnedMs = 60 * 60 * 1000;
  agg.lastPaperPage = 150;
  agg.paperPagesTotal = 200;
  // 50 pages left at 50 pages an hour.
  EXPECT_EQ(secondsToFinish(agg), 3600u);
  // Without the book's length: from the time per percent so far.
  agg.paperPagesTotal = 0;
  agg.hasFirst = true;
  agg.firstPercentBp = 1000;
  agg.totals.lastPercentBp = 3000;
  agg.totals.totalActiveSec = 2 * 3600;
  EXPECT_EQ(secondsToFinish(agg), 7u * 3600u);
  agg.totals.finishedUtc = 1;
  EXPECT_EQ(secondsToFinish(agg), 0u);
}

TEST(ReadingStatsBook, PaperPagesTotalFromSessions) {
  BookAggregate agg;
  SessionRecord s = session(1767225600, 1800, 1500);
  s.paperPagesTotal = 300;
  addBookSession(agg, s, true, nullptr);
  SessionRecord later = session(1767225600 + 86400, 1800, 1500);  // length not known this time
  addBookSession(agg, later, true, nullptr);
  EXPECT_EQ(agg.paperPagesTotal, 300);
}

TEST(ReadingStatsBook, DaysToFinishInLocalDays) {
  BookAggregate agg;
  EXPECT_EQ(daysToFinish(agg), 0);
  // Mon 2026-01-05 23:30 local (UTC+1): 22:30 UTC.
  SessionRecord first = session(static_cast<uint32_t>(day(2026, 1, 5)) * 86400 + 22 * 3600 + 1800, 1200, 1100);
  first.tzOffsetMin = 60;
  addBookSession(agg, first, true, nullptr);
  // Finished the next evening: two local days, though less than 24 h apart.
  SessionRecord last = session(static_cast<uint32_t>(day(2026, 1, 6)) * 86400 + 18 * 3600, 1200, 1100);
  last.tzOffsetMin = 60;
  last.flags |= SESSION_FINISHED;
  addBookSession(agg, last, true, nullptr);
  EXPECT_EQ(daysToFinish(agg), 2);
  // Finished at an unknown time.
  BookAggregate undated;
  addBookSession(undated, session(0, 600, 500, SESSION_FINISHED), true, nullptr);
  EXPECT_EQ(daysToFinish(undated), 0);
}

TEST(ReadingStatsBook, ProgressPointsStayBounded) {
  BookAggregate agg;
  std::vector<ProgressPoint> points;
  for (int d = 0; d < 400; ++d) {
    SessionRecord s = session(1767225600 + static_cast<uint32_t>(d) * 86400, 1200, 1100);
    s.endPercentBp = static_cast<uint16_t>(d * 20);
    addBookSession(agg, s, true, &points);
    ASSERT_LE(points.size(), MAX_PROGRESS_POINTS + 1);
  }
  EXPECT_EQ(points.front().day, day(2026, 1, 1));  // the first day stays
  EXPECT_EQ(points.back().percentBp, 399 * 20);    // and the last
  for (size_t i = 1; i < points.size(); ++i) EXPECT_LT(points[i - 1].day, points[i].day);
}

TEST(ReadingStatsBook, ProgressColumnsCarryForward) {
  const std::vector<ProgressPoint> points = {{100, 500}, {103, 2000}, {109, 4000}};
  uint16_t columns[5] = {};
  progressColumns(points.data(), points.size(), 100, 109, 5, columns);  // two days per column
  EXPECT_EQ(columns[0], 500);
  EXPECT_EQ(columns[1], 2000);
  EXPECT_EQ(columns[2], 2000);
  EXPECT_EQ(columns[3], 2000);
  EXPECT_EQ(columns[4], 4000);
}
