#include "StatsInsights.h"

#include <algorithm>
#include <cstring>

namespace stats {

static_assert(sizeof(ChapterStats) == 36);

namespace {
PagePos replayPos(const uint16_t spine, const uint32_t offset) {
  PagePos pos;
  pos.spine = spine;
  pos.offset = offset == UNKNOWN_OFFSET ? 0 : offset;
  pos.offsetKnown = offset != UNKNOWN_OFFSET;
  return pos;
}

uint16_t inc16(const uint16_t value) { return value == 0xFFFF ? value : static_cast<uint16_t>(value + 1); }

uint16_t add16(const uint16_t a, const uint16_t b) {
  return static_cast<uint16_t>(std::min<uint32_t>(0xFFFF, static_cast<uint32_t>(a) + b));
}

// Minutes after LATE_END_HOUR: the order of the latest-night and earliest-morning records.
int afterDayStart(const int minuteOfDay) { return (minuteOfDay - LATE_END_HOUR * 60 + 24 * 60) % (24 * 60); }

bool isLateHour(const int hour) { return hour >= LATE_START_HOUR || hour < LATE_END_HOUR; }

void setRecord(RecordRef& record, const uint32_t value, const SessionRecord& session) {
  const bool dated = hasWallTime(session);
  record.value = value;
  record.utc = dated ? session.startUtc : 0;
  record.tzOffsetMin = dated ? session.tzOffsetMin : 0;
  record.book = session.bookIndex;
}

void addSpeed(SpeedCell& cell, const SessionRecord& session) {
  cell.charsRead += session.charsRead;
  cell.learnedMs += session.learnedMs;
}
}  // namespace

// --- Time of day ----------------------------------------------------------------------------

int localMinuteOfDay(const uint32_t utc, const int16_t tzOffsetMin) {
  const int64_t local = static_cast<int64_t>(utc) + static_cast<int64_t>(tzOffsetMin) * 60;
  return static_cast<int>((local - static_cast<int64_t>(localDay(utc, tzOffsetMin)) * 86400) / 60);
}

uint32_t lateNightSec(const SessionRecord& session) {
  uint32_t late = 0;
  spreadOverHours(
      session,
      [](void* ctx, int32_t, const int hour, const uint32_t sec) {
        if (isLateHour(hour)) *static_cast<uint32_t*>(ctx) += sec;
      },
      &late);
  return late;
}

uint32_t lookupsPerHourTenths(const uint32_t lookups, const uint32_t activeSec) {
  if (activeSec == 0) return 0;
  return static_cast<uint32_t>((static_cast<uint64_t>(lookups) * 36000 + activeSec / 2) / activeSec);
}

DayPart dayPartOf(const int minuteOfDay) {
  const int hour = minuteOfDay / 60;
  if (hour >= NIGHT_START_HOUR || hour < MORNING_START_HOUR) return DAY_NIGHT;
  if (hour >= EVENING_START_HOUR) return DAY_EVENING;
  if (hour >= AFTERNOON_START_HOUR) return DAY_AFTERNOON;
  return DAY_MORNING;
}

int busiestIndex(const uint32_t* values, const int count) {
  int best = -1;
  for (int i = 0; i < count; ++i) {
    if (values[i] > 0 && (best < 0 || values[i] > values[best])) best = i;
  }
  return best;
}

void gridTotals(const uint32_t* grid, uint32_t* weekdays, uint32_t* hours) {
  std::fill(weekdays, weekdays + 7, 0u);
  std::fill(hours, hours + 24, 0u);
  for (int d = 0; d < 7; ++d) {
    for (int h = 0; h < 24; ++h) {
      weekdays[d] += grid[d * 24 + h];
      hours[h] += grid[d * 24 + h];
    }
  }
}

// --- Records --------------------------------------------------------------------------------

void addRecordSession(Records& records, std::vector<RecordsBook>& books, const SessionRecord& session) {
  if (session.bookIndex >= books.size()) return;
  RecordsBook& book = books[session.bookIndex];
  // Records without a time of day take undated sessions too.
  if (session.activeSec > records.longestSession.value) setRecord(records.longestSession, session.activeSec, session);
  if (session.lookups > records.mostLookups.value) setRecord(records.mostLookups, session.lookups, session);
  book.charsRead += session.charsRead;
  book.learnedMs += session.learnedMs;
  records.lookups += session.lookups;
  records.activeSec += session.activeSec;
  if (!hasWallTime(session)) {
    book.lastEndUtc = 0;  // an unknown time breaks the sitting, as in applySession
    return;
  }

  if (book.sittingSec > 0 && sameSitting(book.lastEndUtc, session.startUtc)) {
    book.sittingSec += session.activeSec;
  } else {
    book.sittingSec = session.activeSec;
    book.sittingUtc = session.startUtc;
    book.sittingTz = session.tzOffsetMin;
  }
  book.lastEndUtc = session.startUtc + session.durationSec;
  if (book.sittingSec > records.longestSitting.value) {
    records.longestSitting = {book.sittingSec, book.sittingUtc, book.sittingTz, session.bookIndex};
  }
  // A day or longer has no meaningful end time.
  if (session.durationSec < 86400) {
    const int startMinute = localMinuteOfDay(session.startUtc, session.tzOffsetMin);
    // A session that reads on past LATE_END_HOUR ends at 03:59 for the latest night.
    const int endAfter = std::min<int>(afterDayStart(startMinute) + static_cast<int>(session.durationSec / 60),
                                       24 * 60 - 1);
    if (records.latestNight.book == NO_BOOK ||
        endAfter > afterDayStart(static_cast<int>(records.latestNight.value))) {
      setRecord(records.latestNight, static_cast<uint32_t>((endAfter + LATE_END_HOUR * 60) % (24 * 60)), session);
    }
    if (records.earliestMorning.book == NO_BOOK ||
        afterDayStart(startMinute) < afterDayStart(static_cast<int>(records.earliestMorning.value))) {
      setRecord(records.earliestMorning, static_cast<uint32_t>(startMinute), session);
    }
  }
  const uint32_t late = lateNightSec(session);
  if (late > 0) {
    ++records.lateSessions;
    records.lateSec += late;
  }
}

void finishRecords(Records& records, const std::vector<RecordsBook>& books, const DayRecord* days,
                   const size_t dayCount) {
  uint32_t weekSec = 0;
  int32_t week = 0;
  for (size_t i = 0; i < dayCount; ++i) {
    if (days[i].activeSec > records.mostDaySec) {
      records.mostDaySec = days[i].activeSec;
      records.mostDay = days[i].day;
    }
    const int32_t dayWeek = weekIndex(days[i].day);
    if (i == 0 || dayWeek != week) {
      week = dayWeek;
      weekSec = 0;
    }
    weekSec += days[i].activeSec;
    if (weekSec > records.mostWeekSec) {
      records.mostWeekSec = weekSec;
      records.mostWeek = week;
    }
  }
  records.longestStreak = computeStreaks(days, dayCount, 0).longest;
  for (size_t i = 0; i < books.size(); ++i) {
    if (books[i].learnedMs < FASTEST_MIN_MEASURED_MS) continue;
    const uint32_t speed = pagesPerHour(books[i].charsRead, books[i].learnedMs);
    if (speed > records.fastestBook.value) records.fastestBook = {speed, 0, 0, static_cast<uint16_t>(i)};
  }
}

// --- Year in review -------------------------------------------------------------------------

void addYearSession(YearReview& review, std::vector<YearBook>& books, const SessionRecord& session,
                    const bool ignoreShort, const bool hidden) {
  if (session.bookIndex >= books.size()) return;
  YearBook& book = books[session.bookIndex];
  if (finishesBook(session)) book.finished = true;
  if (!countsInTotals(session, ignoreShort)) return;
  addStyleSession(book.style, session);
  if (!countsInCalendar(session, ignoreShort, hidden)) return;
  int year;
  unsigned month, dayOfMonth;
  civilFromDays(localDay(session.startUtc, session.tzOffsetMin), year, month, dayOfMonth);
  if (year != review.year) return;
  book.yearSec += session.activeSec;
  review.activeSec += session.activeSec;
  ++review.sessions;
  review.lookups += session.lookups;
  review.months[month - 1] += session.activeSec;
  addToHourGrid(review.grid, session);
  const uint32_t late = lateNightSec(session);
  if (late > 0) {
    ++review.lateSessions;
    review.lateSec += late;
  }
}

void finishYearReview(YearReview& review, const std::vector<YearBook>& books, const DayRecord* days,
                      const size_t dayCount, const bool todayKnown, const int32_t today) {
  const int32_t first = daysFromCivil(review.year, 1, 1);
  const int32_t last = daysFromCivil(review.year + 1, 1, 1) - 1;
  const DayRecord* begin = std::lower_bound(days, days + dayCount, first,
                                            [](const DayRecord& d, const int32_t day) { return d.day < day; });
  const DayRecord* end = std::upper_bound(days, days + dayCount, last,
                                          [](const int32_t day, const DayRecord& d) { return day < d.day; });
  review.longestStreak = computeStreaks(begin, static_cast<size_t>(end - begin), 0).longest;

  std::fill(review.styles, review.styles + STYLE_COUNT, 0);
  review.topCount = 0;
  for (size_t i = 0; i < books.size(); ++i) {
    const YearBook& book = books[i];
    if (book.yearSec == 0) continue;
    const auto style = static_cast<int>(classifyStyle(book.style, book.finished, todayKnown, today));
    review.styles[style] = inc16(review.styles[style]);
    // Insertion into the top list, largest first; ties keep the lower index.
    int at = review.topCount;
    while (at > 0 && book.yearSec > review.topSec[at - 1]) --at;
    if (at >= YEAR_TOP_BOOKS) continue;
    for (int k = std::min(review.topCount, YEAR_TOP_BOOKS - 1); k > at; --k) {
      review.topSec[k] = review.topSec[k - 1];
      review.topBooks[k] = review.topBooks[k - 1];
    }
    review.topSec[at] = book.yearSec;
    review.topBooks[at] = static_cast<uint16_t>(i);
    review.topCount = std::min(review.topCount + 1, YEAR_TOP_BOOKS);
  }
}

// --- Speed trends ---------------------------------------------------------------------------

uint32_t cellPagesPerHour(const SpeedCell& cell) {
  uint64_t chars = cell.charsRead;
  uint64_t ms = cell.learnedMs;
  while (chars > UINT32_MAX || ms > UINT32_MAX) {
    chars >>= 1;
    ms >>= 1;
  }
  return pagesPerHour(static_cast<uint32_t>(chars), static_cast<uint32_t>(ms));
}

void addSpeedSession(SpeedTrends& trends, const SessionRecord& session) {
  if (!hasWallTime(session) || session.learnedMs == 0) return;
  const int32_t month = monthIndex(localDay(session.startUtc, session.tzOffsetMin)) - trends.firstMonth;
  if (month >= 0 && month < SPEED_MONTHS) addSpeed(trends.months[month], session);
  addSpeed(trends.dayParts[dayPartOf(localMinuteOfDay(session.startUtc, session.tzOffsetMin))], session);
  if (session.fontSize == 0) return;
  int at = 0;
  while (at < trends.fontCount && trends.fontSizes[at] < session.fontSize) ++at;
  if (at == trends.fontCount || trends.fontSizes[at] != session.fontSize) {
    if (trends.fontCount >= MAX_FONT_SIZES) return;
    for (int k = trends.fontCount; k > at; --k) {
      trends.fontSizes[k] = trends.fontSizes[k - 1];
      trends.fonts[k] = trends.fonts[k - 1];
    }
    trends.fontSizes[at] = session.fontSize;
    trends.fonts[at] = SpeedCell{};
    ++trends.fontCount;
  }
  addSpeed(trends.fonts[at], session);
}

// --- What hooked me -------------------------------------------------------------------------

void mergeChapter(ChapterStats& a, const ChapterStats& b) {
  a.activeMs += b.activeMs;
  a.charsRead += b.charsRead;
  a.learnedMs += b.learnedMs;
  a.maxOffset = std::max(a.maxOffset, b.maxOffset);
  a.rereadChars += b.rereadChars;
  a.starts = add16(a.starts, b.starts);
  a.ends = add16(a.ends, b.ends);
  a.pauses = add16(a.pauses, b.pauses);
  a.turnsBack = add16(a.turnsBack, b.turnsBack);
  a.oneMore = add16(a.oneMore, b.oneMore);
  a.lookups = add16(a.lookups, b.lookups);
}

void ChapterReplay::beginCount(const float bookRate, const uint16_t typicalPageChars, const uint32_t skip,
                               const bool trim, const bool ignoreShortOpens) {
  rate = bookRate;
  typicalChars = typicalPageChars;
  skipSeq = skip;
  idleTrim = trim;
  ignoreShort = ignoreShortOpens;
  memset(countedBits, 0, sizeof(countedBits));
  sessions = 0;
  maxSpine = 0;
  replay.begin(0, rate, typicalChars, skipSeq, idleTrim);
}

void ChapterReplay::count(const uint8_t* record, const size_t size) {
  SessionRecord closed{};
  const bool emitted = replay.feed(record, size, closed);
  // A start record closes the session before it; an end record the open one: sessions - 1 both times.
  if (emitted && sessions > 0 && sessions - 1 < MAX_HOOK_SESSIONS && countsInTotals(closed, ignoreShort)) {
    countedBits[(sessions - 1) / 8] |= static_cast<uint8_t>(1u << ((sessions - 1) % 8));
  }
  if (record[0] == RAW_START) {
    RawStart start{};
    if (!readRawRecord(record, size, &start, sizeof(start))) return;
    ++sessions;
    maxSpine = std::max(maxSpine, start.spine);
  } else if (record[0] == RAW_TURN) {
    RawTurn turn{};
    if (readRawRecord(record, size, &turn, sizeof(turn))) maxSpine = std::max(maxSpine, turn.spine);
  }
}

void ChapterReplay::finishCount() {
  SessionRecord closed{};
  if (replay.finish(closed) && sessions > 0 && sessions - 1 < MAX_HOOK_SESSIONS &&
      countsInTotals(closed, ignoreShort)) {
    countedBits[(sessions - 1) / 8] |= static_cast<uint8_t>(1u << ((sessions - 1) % 8));
  }
}

size_t ChapterReplay::chapterCount() const {
  return sessions == 0 ? 0 : std::min<size_t>(static_cast<size_t>(maxSpine) + 1, MAX_CHAPTERS);
}

bool ChapterReplay::isCounted(const size_t ordinal) const {
  return ordinal >= MAX_HOOK_SESSIONS || (countedBits[ordinal / 8] & (1u << (ordinal % 8))) != 0;
}

uint16_t ChapterReplay::chapterIndex(const uint16_t spine) const {
  return static_cast<uint16_t>(std::min<size_t>(spine, chapterTotal - 1));
}

int ChapterReplay::chapterKey(const uint16_t spine) const {
  const uint16_t index = chapterIndex(spine);
  return toc ? toc[index] : index;
}

void ChapterReplay::beginChapters(ChapterStats* out, const size_t count, const int16_t* tocOfSpine) {
  chapters = out;
  chapterTotal = count;
  toc = tocOfSpine;
  for (size_t i = 0; i < count; ++i) {
    chapters[i] = ChapterStats{};
    chapters[i].spine = static_cast<uint16_t>(i);
  }
  hookTotals = HookTotals{};
  sessions = 0;
  open = false;
  replay.begin(0, rate, typicalChars, skipSeq, idleTrim);
}

void ChapterReplay::addShare(const uint16_t spine, const uint32_t ms) {
  const uint16_t index = chapterIndex(spine);
  for (size_t i = 0; i < shareCount; ++i) {
    if (shares[i].chapter == index) {
      shares[i].ms += ms;
      return;
    }
  }
  if (shareCount < MAX_SESSION_CHAPTERS) {
    shares[shareCount++] = {index, ms};
  } else {
    shares[MAX_SESSION_CHAPTERS - 1].ms += ms;
  }
}

uint32_t ChapterReplay::openStretchMs(const uint32_t endMs, const bool trimIdle) const {
  const uint32_t now = replay.sessionClock().activeMsAt(endMs, trimIdle);
  return now > clockAtTurnMs ? now - clockAtTurnMs : 0;
}

void ChapterReplay::closeSession(const SessionRecord* closed, const uint32_t openMs, const bool hasEnd) {
  if (!open) return;
  open = false;
  if (!counted) return;
  uint64_t sum = 0;
  for (size_t i = 0; i < shareCount; ++i) sum += shares[i].ms;
  // Without an end record (the running session, a reset) there is no recorded total: the dwells
  // and the open stretch stand as they are.
  const bool useTotal = closed && hasEnd;
  const uint64_t total = useTotal ? static_cast<uint64_t>(closed->activeSec) * 1000 : sum + openMs;
  // With every page dwell recorded, the last page is exactly what the session total leaves.
  const uint64_t last = exact && useTotal ? (total > sum ? total - sum : 0) : openMs;
  addShare(pos.spine, static_cast<uint32_t>(last));
  sum += last;
  if (sum == 0) {
    chapters[chapterIndex(pos.spine)].activeMs += static_cast<uint32_t>(total);
  } else {
    // Scale the shares to the session's active time; the rounding goes to the largest share.
    uint64_t given = 0;
    size_t largest = 0;
    for (size_t i = 0; i < shareCount; ++i) {
      const uint64_t part = shares[i].ms * total / sum;
      chapters[shares[i].chapter].activeMs += static_cast<uint32_t>(part);
      given += part;
      if (shares[i].ms > shares[largest].ms) largest = i;
    }
    chapters[shares[largest].chapter].activeMs += static_cast<uint32_t>(total - given);
  }
  shareCount = 0;
  ChapterStats& end = chapters[chapterIndex(pos.spine)];
  end.ends = inc16(end.ends);
  ++hookTotals.sessions;
  if (sessionOneMore) ++hookTotals.oneMoreSessions;
  hookTotals.lastEndSpine = pos.spine;
  hookTotals.hasLastEnd = true;
}

void ChapterReplay::feed(const uint8_t* record, const size_t size) {
  if (chapterTotal == 0) return;
  RawStart start{};
  RawEnd end{};
  const bool isStart = record[0] == RAW_START && readRawRecord(record, size, &start, sizeof(start));
  const bool isEnd = record[0] == RAW_END && readRawRecord(record, size, &end, sizeof(end));
  // A start record resets the clock of the session it closes: read the clock first.
  uint32_t openMs = 0;
  if (open && isStart) openMs = openStretchMs(lastMs, false);
  if (open && isEnd) openMs = openStretchMs(end.durationMs, end.reason == END_AUTO_SLEEP);
  const bool wasPaused = replay.sessionClock().isPaused();

  SessionRecord closed{};
  const bool emitted = replay.feed(record, size, closed);
  const ActiveClock& clock = replay.sessionClock();
  if (isStart) {
    closeSession(emitted ? &closed : nullptr, openMs, false);
    counted = isCounted(sessions++);
    open = true;
    exact = true;
    pos.spine = start.spine;
    pos.offset = start.offset == UNKNOWN_OFFSET ? 0 : start.offset;
    pos.offsetKnown = start.offset != UNKNOWN_OFFSET;
    lastMs = start.tMs;
    clockAtTurnMs = 0;
    dictAtTurnMs = 0;
    shareCount = 0;
    armed = false;
    sessionOneMore = false;
    if (counted) {
      ChapterStats& here = chapters[chapterIndex(start.spine)];
      here.starts = inc16(here.starts);
    }
    return;
  }
  if (isEnd) {
    if (end.flags & SESSION_RECORDS_DROPPED) exact = false;
    closeSession(emitted ? &closed : nullptr, openMs, true);
    return;
  }
  if (!open) return;

  if (record[0] == RAW_TURN) {
    RawTurn turn{};
    if (!readRawRecord(record, size, &turn, sizeof(turn))) return;
    lastMs = std::max(lastMs, turn.tMs);
    const PagePos to = replayPos(turn.spine, turn.offset);
    if (counted) {
      const ActiveClock::Turn& result = replay.lastTurn();
      const uint32_t dictMs = clock.dictionaryMsAt(turn.tMs);
      const uint32_t dwellMs = turn.dwellDs != 0 ? turn.dwellDs * 100u : result.countedMs;
      if (turn.dwellDs == 0) exact = false;
      addShare(pos.spine, dwellMs + (dictMs - dictAtTurnMs));
      dictAtTurnMs = dictMs;
      ChapterStats& from = chapters[chapterIndex(pos.spine)];
      if (result.charsKnown) {
        from.charsRead += result.chars;
        from.learnedMs += dwellMs;
        if (pos.offset < from.maxOffset) from.rereadChars += std::min(to.offset, from.maxOffset) - pos.offset;
        from.maxOffset = std::max(from.maxOffset, to.offset);
      }
      if (turn.kind == TURN_PREV || turn.kind == TURN_JUMP_BACK) from.turnsBack = inc16(from.turnsBack);
      // "One more chapter": reading on into the next TOC entry, then ONE_MORE_PAGES more pages.
      const int fromKey = chapterKey(pos.spine);
      const int toKey = chapterKey(to.spine);
      if (turn.kind == TURN_NEXT && to.spine == pos.spine + 1 && fromKey >= 0 && toKey >= 0 && toKey != fromKey) {
        armed = true;
        enteredSpine = to.spine;
        enteredKey = toKey;
        pagesInto = 0;
      } else if (armed && toKey == enteredKey) {
        if (turn.kind == TURN_NEXT && ++pagesInto >= ONE_MORE_PAGES) {
          ChapterStats& entered = chapters[chapterIndex(enteredSpine)];
          entered.oneMore = inc16(entered.oneMore);
          sessionOneMore = true;
          armed = false;
        }
      } else {
        armed = false;
      }
    }
    clockAtTurnMs = clock.activeMsAt(turn.tMs);
    pos = to;
    return;
  }
  if (record[0] == RAW_EVENT) {
    RawEvent event{};
    if (!readRawRecord(record, size, &event, sizeof(event))) return;
    lastMs = std::max(lastMs, event.tMs);
    if (!counted) return;
    ChapterStats& here = chapters[chapterIndex(pos.spine)];
    if (event.kind == EVENT_LOOKUP) here.lookups = inc16(here.lookups);
    if (event.kind == EVENT_PAUSE && !wasPaused) here.pauses = inc16(here.pauses);
  }
}

void ChapterReplay::finish() {
  const uint32_t openMs = open ? openStretchMs(lastMs, false) : 0;
  SessionRecord closed{};
  const bool emitted = replay.finish(closed);
  closeSession(emitted ? &closed : nullptr, openMs, false);
}

HookHighlights pickHighlights(const ChapterStats* chapters, const size_t count) {
  HookHighlights h;
  const auto most = [chapters](int& best, const size_t i, uint32_t (*get)(const ChapterStats&)) {
    const uint32_t value = get(chapters[i]);
    if (value > 0 && (best < 0 || value > get(chapters[best]))) best = static_cast<int>(i);
  };
  uint32_t fastSpeed = 0;
  uint32_t slowSpeed = 0;
  for (size_t i = 0; i < count; ++i) {
    most(h.mostTime, i, [](const ChapterStats& c) { return c.activeMs; });
    most(h.mostStarts, i, [](const ChapterStats& c) { return static_cast<uint32_t>(c.starts); });
    most(h.mostEnds, i, [](const ChapterStats& c) { return static_cast<uint32_t>(c.ends); });
    most(h.mostReread, i, [](const ChapterStats& c) { return c.rereadChars; });
    most(h.mostBack, i, [](const ChapterStats& c) { return static_cast<uint32_t>(c.turnsBack); });
    most(h.mostPauses, i, [](const ChapterStats& c) { return static_cast<uint32_t>(c.pauses); });
    most(h.mostLookups, i, [](const ChapterStats& c) { return static_cast<uint32_t>(c.lookups); });
    const uint32_t speed = pagesPerHour(chapters[i].charsRead, chapters[i].learnedMs);
    if (speed == 0) continue;
    if (h.fastest < 0 || speed > fastSpeed) {
      h.fastest = static_cast<int>(i);
      fastSpeed = speed;
    }
    if (h.slowest < 0 || speed < slowSpeed) {
      h.slowest = static_cast<int>(i);
      slowSpeed = speed;
    }
  }
  return h;
}

}  // namespace stats
