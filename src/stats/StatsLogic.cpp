#include "StatsLogic.h"

#include <algorithm>
#include <cmath>
#include <ctime>

namespace stats {

namespace {
int32_t floorDiv(const int64_t a, const int64_t b) {
  int64_t q = a / b;
  if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
  return static_cast<int32_t>(q);
}

uint16_t addSat16(const uint16_t a, const uint32_t b) {
  return static_cast<uint16_t>(std::min<uint32_t>(0xFFFF, a + b));
}

PagePos replayPos(const uint16_t spine, const uint32_t offset) {
  PagePos pos;
  pos.spine = spine;
  pos.offset = offset == UNKNOWN_OFFSET ? 0 : offset;
  pos.offsetKnown = offset != UNKNOWN_OFFSET;
  return pos;
}
}  // namespace

int rateBucket(const float secPerKChar) {
  if (!(secPerKChar > 1.0f)) return 0;
  const int bucket = static_cast<int>(std::floor(std::log2(secPerKChar) * BUCKETS_PER_OCTAVE));
  return std::clamp(bucket, 0, RATE_BUCKETS - 1);
}

void addRateSample(uint16_t* hist, uint32_t& samples, const float secPerKChar) {
  const int bucket = rateBucket(secPerKChar);
  if (hist[bucket] == 0xFFFF) {
    for (int i = 0; i < RATE_BUCKETS; ++i) hist[i] = static_cast<uint16_t>(hist[i] / 2);
    samples /= 2;
  }
  ++hist[bucket];
  ++samples;
}

void mergeRateHist(uint16_t* into, uint32_t& intoSamples, const uint16_t* add, const uint32_t addSamples) {
  bool overflow = false;
  for (int i = 0; i < RATE_BUCKETS; ++i) overflow |= static_cast<uint32_t>(into[i]) + add[i] > 0xFFFF;
  if (overflow) {
    for (int i = 0; i < RATE_BUCKETS; ++i) into[i] = static_cast<uint16_t>(into[i] / 2);
    intoSamples /= 2;
  }
  for (int i = 0; i < RATE_BUCKETS; ++i) into[i] = addSat16(into[i], add[i]);
  intoSamples += addSamples;
}

float medianRate(const uint16_t* hist) {
  uint32_t total = 0;
  for (int i = 0; i < RATE_BUCKETS; ++i) total += hist[i];
  if (total == 0) return 0.0f;
  const uint32_t half = (total + 1) / 2;
  uint32_t seen = 0;
  for (int i = 0; i < RATE_BUCKETS; ++i) {
    seen += hist[i];
    if (seen >= half) return std::exp2((static_cast<float>(i) + 0.5f) / BUCKETS_PER_OCTAVE);
  }
  return 0.0f;
}

float chooseRate(const uint16_t* bookHist, const uint32_t bookSamples, const uint16_t* globalHist,
                 const uint32_t globalSamples) {
  if (bookSamples >= MIN_MODEL_SAMPLES) return medianRate(bookHist);
  if (globalSamples >= MIN_MODEL_SAMPLES) return medianRate(globalHist);
  return 0.0f;
}

uint32_t dwellCapSec(const float rate, const uint16_t pageChars) {
  if (!(rate > 0.0f)) return DEFAULT_CAP_SEC;
  const float cap = CAP_FACTOR * rate * static_cast<float>(pageChars) / 1000.0f;
  if (cap <= static_cast<float>(MIN_CAP_SEC)) return MIN_CAP_SEC;
  if (cap >= static_cast<float>(MAX_CAP_SEC)) return MAX_CAP_SEC;
  return static_cast<uint32_t>(cap);
}

uint8_t classifyTurn(const PagePos& from, const PagePos& to) {
  if (from.spine == to.spine) {
    const bool offsetsKnown = from.offsetKnown && to.offsetKnown;
    if (to.page == from.page && (!offsetsKnown || to.offset == from.offset)) return 0;
    if (to.page == from.page + 1) return TURN_NEXT;
    if (to.page + 1 == from.page) return TURN_PREV;
    const bool forward = offsetsKnown && to.offset != from.offset ? to.offset > from.offset : to.page > from.page;
    return forward ? TURN_JUMP_FWD : TURN_JUMP_BACK;
  }
  if (to.spine == from.spine + 1 && to.page == 0) return TURN_NEXT;
  if (to.spine + 1 == from.spine && to.pageCount > 0 && to.page + 1 == to.pageCount) return TURN_PREV;
  return to.spine > from.spine ? TURN_JUMP_FWD : TURN_JUMP_BACK;
}

void ActiveClock::begin(const float newRate, const uint16_t typicalPageChars) {
  rate = newRate;
  typicalChars = typicalPageChars >= MIN_LEARN_CHARS ? typicalPageChars : DEFAULT_PAGE_CHARS;
  started = false;
  paused = false;
  lastMs = 0;
  pausedAtMs = 0;
  activeMs = 0;
}

uint32_t ActiveClock::capMs(const uint16_t chars) const { return dwellCapSec(rate, chars) * 1000; }

void ActiveClock::start(const uint32_t nowMs, const PagePos& pos) {
  started = true;
  paused = false;
  last = pos;
  lastMs = nowMs;
}

ActiveClock::Turn ActiveClock::pageShown(const uint32_t nowMs, const PagePos& pos) {
  if (!started) {
    start(nowMs, pos);
    return {};
  }
  const uint8_t kind = classifyTurn(last, pos);
  if (kind == 0) return {};  // a repaint: the dwell goes on
  return turn(nowMs, kind, pos);
}

ActiveClock::Turn ActiveClock::turn(const uint32_t nowMs, const uint8_t kind, const PagePos& pos) {
  Turn result;
  result.kind = kind;
  if (!started) {
    start(nowMs, pos);
    return result;
  }
  resume(nowMs);
  const uint32_t dwellMs = nowMs - lastMs;
  result.charsKnown =
      kind == TURN_NEXT && last.spine == pos.spine && last.offsetKnown && pos.offsetKnown && pos.offset > last.offset;
  result.chars = result.charsKnown ? static_cast<uint16_t>(std::min<uint32_t>(pos.offset - last.offset, 0xFFFF)) : 0;
  result.countedMs = std::min(dwellMs, capMs(result.charsKnown ? result.chars : typicalChars));
  activeMs += result.countedMs;
  if (result.charsKnown && result.chars >= MIN_LEARN_CHARS) {
    typicalChars = static_cast<uint16_t>(typicalChars + (static_cast<int>(result.chars) - typicalChars) / 8);
    if (dwellMs >= MIN_LEARN_DWELL_MS && dwellMs <= MAX_LEARN_DWELL_MS) {
      result.learned = true;
      result.rate = static_cast<float>(dwellMs) / static_cast<float>(result.chars);  // ms per char = s per 1000
    }
  }
  last = pos;
  lastMs = nowMs;
  return result;
}

void ActiveClock::pause(const uint32_t nowMs) {
  if (!started || paused) return;
  paused = true;
  pausedAtMs = nowMs;
}

void ActiveClock::resume(const uint32_t nowMs) {
  if (!paused) return;
  paused = false;
  lastMs += nowMs - pausedAtMs;  // the dwell goes on as if the pause had not happened
}

uint32_t ActiveClock::activeMsAt(const uint32_t nowMs) const {
  if (!started) return activeMs;
  const uint32_t end = paused ? pausedAtMs : nowMs;
  return activeMs + std::min(end - lastMs, capMs(typicalChars));
}

bool isShortOpen(const uint32_t activeSec, const uint32_t turns) { return activeSec < SHORT_OPEN_SEC || turns == 0; }

bool sameSitting(const uint32_t prevEndUtc, const uint32_t startUtc) {
  if (prevEndUtc == 0 || startUtc == 0) return false;
  return startUtc <= prevEndUtc || startUtc - prevEndUtc < SITTING_GAP_SEC;
}

bool countsInTotals(const SessionRecord& session, const bool ignoreShort) {
  return !(ignoreShort && (session.flags & SESSION_SHORT));
}

bool hasWallTime(const SessionRecord& session) { return (session.flags & SESSION_WALL_KNOWN) && session.startUtc != 0; }

bool countsInCalendar(const SessionRecord& session, const bool ignoreShort, const bool bookExcluded) {
  return countsInTotals(session, ignoreShort) && hasWallTime(session) && !bookExcluded;
}

bool finishesBook(const SessionRecord& session) { return (session.flags & SESSION_FINISHED) != 0; }

bool crossesFinish(const uint16_t startPercentBp, const uint16_t percentBp) {
  return startPercentBp < FINISHED_BP && percentBp >= FINISHED_BP;
}

bool LogScanner::next(uint8_t* buf, size_t& size) {
  if (read(ctx, buf, 2) != 2) return false;
  const size_t recordSize = buf[1];
  if (recordSize < 2) return false;  // not a record: treat as the end
  const size_t keep = std::min(recordSize, RAW_MAX_KNOWN_SIZE);
  if (read(ctx, buf + 2, keep - 2) != keep - 2) return false;
  uint8_t skip[16];
  for (size_t left = recordSize - keep; left > 0;) {
    const size_t n = std::min(left, sizeof(skip));
    if (read(ctx, skip, n) != n) return false;
    left -= n;
  }
  end += recordSize;
  size = keep;
  return true;
}

void LogReplay::begin(const uint16_t book, const float bookRate, const uint16_t typicalPageChars, const uint32_t skip) {
  bookIndex = book;
  rate = bookRate;
  typicalChars = typicalPageChars;
  skipSeq = skip;
  open = false;
}

bool LogReplay::close(const RawEnd* end, SessionRecord& out) {
  if (!open) return false;
  open = false;
  if (end) {
    rec.durationSec = end->durationMs / 1000;
    rec.activeSec = end->activeSec;
    rec.endReason = end->reason;
    rec.flags = end->flags | (rec.flags & (SESSION_WALL_KNOWN | SESSION_FINISHED));
    rec.turnsFwd = end->turnsFwd;
    rec.turnsBack = end->turnsBack;
    rec.jumps = end->jumps;
    rec.lookups = end->lookups;
    rec.clippings = end->clippings;
    rec.bookmarks = end->bookmarks;
    rec.endPaperPage = end->endPaperPage;
    rec.endPercentBp = end->endPercentBp;
    rec.endSpine = end->endSpine;
    rec.charsRead = end->charsRead;
    rec.learnedMs = end->learnedMs;
    if (rec.startUtc == 0 && end->endUtc > rec.durationSec) {
      // The clock became trusted during the session: date it from the end.
      rec.startUtc = end->endUtc - rec.durationSec;
      rec.tzOffsetMin = end->tzOffsetMin;
      rec.flags |= SESSION_WALL_KNOWN;
    }
  } else {
    rec.durationSec = lastMs / 1000;
    rec.activeSec = clock.activeMsAt(lastMs) / 1000;
    rec.endReason = END_CRASH;
    rec.flags |= SESSION_CRASHED;
    if (isShortOpen(rec.activeSec, rec.turnsFwd + rec.turnsBack + rec.jumps)) rec.flags |= SESSION_SHORT;
  }
  if (skipSeq != 0 && rec.sessionSeq == skipSeq) return false;
  out = rec;
  return true;
}

bool LogReplay::feed(const uint8_t* record, const size_t size, SessionRecord& out) {
  switch (record[0]) {
    case RAW_START: {
      RawStart start{};
      if (!readRawRecord(record, size, &start, sizeof(start))) return false;
      const bool closed = close(nullptr, out);
      rec = {};
      rec.startUtc = start.startUtc;
      rec.tzOffsetMin = start.tzOffsetMin;
      rec.bookIndex = bookIndex;
      rec.flags = start.flags;
      rec.startSpine = rec.endSpine = start.spine;
      rec.startPercentBp = rec.endPercentBp = start.percentBp;
      rec.startPaperPage = rec.endPaperPage = start.paperPage;
      rec.fontSize = start.fontSize;
      rec.orientation = start.orientation;
      rec.sessionSeq = start.sessionSeq;
      clock.begin(rate, typicalChars);
      clock.start(start.tMs, replayPos(start.spine, start.offset));
      lastMs = start.tMs;
      open = true;
      return closed;
    }
    case RAW_TURN: {
      RawTurn turn{};
      if (!open || !readRawRecord(record, size, &turn, sizeof(turn))) return false;
      lastMs = std::max(lastMs, turn.tMs);
      const auto result = clock.turn(turn.tMs, turn.kind, replayPos(turn.spine, turn.offset));
      if (result.charsKnown) {
        rec.charsRead += result.chars;
        rec.learnedMs += result.countedMs;
      }
      if (turn.kind == TURN_NEXT) {
        ++rec.turnsFwd;
      } else if (turn.kind == TURN_PREV) {
        ++rec.turnsBack;
      } else {
        ++rec.jumps;
      }
      rec.endSpine = turn.spine;
      rec.endPercentBp = turn.percentBp;
      if (crossesFinish(rec.startPercentBp, turn.percentBp)) rec.flags |= SESSION_FINISHED;
      if (turn.paperPage > 0) rec.endPaperPage = turn.paperPage;
      return false;
    }
    case RAW_EVENT: {
      RawEvent event{};
      if (!open || !readRawRecord(record, size, &event, sizeof(event))) return false;
      lastMs = std::max(lastMs, event.tMs);
      switch (event.kind) {
        case EVENT_LOOKUP:
          ++rec.lookups;
          break;
        case EVENT_CLIPPING:
          ++rec.clippings;
          break;
        case EVENT_BOOKMARK:
          ++rec.bookmarks;
          break;
        case EVENT_END_OF_BOOK:
          rec.endPercentBp = END_OF_BOOK_BP;
          rec.flags |= SESSION_FINISHED;
          break;
        case EVENT_PAUSE:
          clock.pause(event.tMs);
          break;
        case EVENT_RESUME:
          clock.resume(event.tMs);
          break;
        default:
          break;
      }
      return false;
    }
    case RAW_END: {
      RawEnd end{};
      if (!readRawRecord(record, size, &end, sizeof(end))) return false;
      return close(&end, out);
    }
    default:
      return false;
  }
}

bool LogReplay::finish(SessionRecord& out) { return close(nullptr, out); }

// Howard Hinnant's days_from_civil / civil_from_days.
int32_t daysFromCivil(int year, const unsigned month, const unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int32_t>(doe) - 719468;
}

void civilFromDays(int32_t days, int& year, unsigned& month, unsigned& day) {
  days += 719468;
  const int era = (days >= 0 ? days : days - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(days - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  day = doy - (153 * mp + 2) / 5 + 1;
  month = mp < 10 ? mp + 3 : mp - 9;
  year = static_cast<int>(yoe) + era * 400 + (month <= 2);
}

int32_t localDay(const uint32_t utc, const int16_t tzOffsetMin) {
  return floorDiv(static_cast<int64_t>(utc) + static_cast<int64_t>(tzOffsetMin) * 60, 86400);
}

int16_t localTzOffsetMin(const uint32_t utc) {
  const time_t t = static_cast<time_t>(utc);
  struct tm local{};
  localtime_r(&t, &local);
  const int64_t localDays = daysFromCivil(local.tm_year + 1900, static_cast<unsigned>(local.tm_mon + 1),
                                          static_cast<unsigned>(local.tm_mday));
  const int64_t localEpoch = localDays * 86400 + local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
  return static_cast<int16_t>((localEpoch - static_cast<int64_t>(utc)) / 60);
}

int weekday(const int32_t day) { return static_cast<int>(day - static_cast<int64_t>(floorDiv(day + 3, 7)) * 7 + 3); }

int32_t weekIndex(const int32_t day) { return floorDiv(static_cast<int64_t>(day) + 3, 7); }

int32_t weekStartDay(const int32_t week) { return week * 7 - 3; }

int32_t monthIndex(const int32_t day) {
  int year;
  unsigned month, dayOfMonth;
  civilFromDays(day, year, month, dayOfMonth);
  return year * 12 + static_cast<int32_t>(month) - 1;
}

int32_t yearOf(const int32_t day) {
  int year;
  unsigned month, dayOfMonth;
  civilFromDays(day, year, month, dayOfMonth);
  return year;
}

ApplyResult applySession(BookTotals& totals, const bool bookExcluded, const SessionRecord& session,
                         const bool ignoreShort) {
  ApplyResult result;
  const bool wallKnown = hasWallTime(session);
  const uint32_t endUtc = wallKnown ? session.startUtc + session.durationSec : 0;
  // A short last session still finishes the book.
  if (totals.finishedUtc == 0 && finishesBook(session)) totals.finishedUtc = wallKnown ? endUtc : 1;
  if (!countsInTotals(session, ignoreShort)) return result;
  result.counted = true;
  result.newSitting = !(wallKnown && sameSitting(totals.lastEndUtc, session.startUtc));

  totals.totalActiveSec += session.activeSec;
  totals.charsRead += session.charsRead;
  totals.learnedMs += session.learnedMs;
  ++totals.sessions;
  if (result.newSitting) ++totals.sittings;
  if (wallKnown) {
    if (totals.firstUtc == 0) totals.firstUtc = session.startUtc;
    totals.lastUtc = std::max(totals.lastUtc, session.startUtc);
  }
  totals.lastEndUtc = endUtc;  // an unknown time breaks the sitting chain
  totals.lastPercentBp = session.endPercentBp;
  totals.maxPercentBp = std::max(totals.maxPercentBp, session.endPercentBp);

  if (countsInCalendar(session, ignoreShort, bookExcluded)) {
    result.calendar = true;
    result.day.day = localDay(session.startUtc, session.tzOffsetMin);
    result.day.activeSec = session.activeSec;
    result.day.sessions = 1;
    result.day.sittings = result.newSitting ? 1 : 0;
    result.day.pagesRead = session.startPaperPage > 0 && session.endPaperPage > session.startPaperPage
                               ? static_cast<uint16_t>(session.endPaperPage - session.startPaperPage)
                               : 0;
  }
  return result;
}

void mergeDay(DayRecord& into, const DayRecord& add) {
  into.activeSec += add.activeSec;
  into.sessions = addSat16(into.sessions, add.sessions);
  into.sittings = addSat16(into.sittings, add.sittings);
  into.pagesRead = addSat16(into.pagesRead, add.pagesRead);
}

void addDay(std::vector<DayRecord>& days, const DayRecord& add) {
  // Sessions arrive almost in order: check the end first.
  if (days.empty() || days.back().day < add.day) {
    days.push_back(add);
    return;
  }
  const auto it = std::lower_bound(days.begin(), days.end(), add.day,
                                   [](const DayRecord& d, const int32_t day) { return d.day < day; });
  if (it != days.end() && it->day == add.day) {
    mergeDay(*it, add);
  } else {
    days.insert(it, add);
  }
}

Streaks computeStreaks(const DayRecord* days, const size_t count, const int32_t today) {
  Streaks streaks;
  int run = 0;
  int32_t prev = 0;
  bool hasPrev = false;
  for (size_t i = 0; i < count; ++i) {
    if (days[i].activeSec == 0) continue;
    run = hasPrev && days[i].day == prev + 1 ? run + 1 : 1;
    prev = days[i].day;
    hasPrev = true;
    streaks.longest = std::max(streaks.longest, run);
  }
  if (hasPrev && (prev == today || prev == today - 1)) streaks.current = run;
  return streaks;
}

int32_t bucketOf(const Range range, const int32_t day) {
  switch (range) {
    case Range::Day:
      return day;
    case Range::Week:
      return weekIndex(day);
    case Range::Month:
      return monthIndex(day);
    case Range::Year:
      return yearOf(day);
  }
  return day;
}

void bucketActive(const DayRecord* days, const size_t dayCount, const Range range, const int32_t firstBucket,
                  const int count, uint32_t* out) {
  std::fill(out, out + count, 0u);
  for (size_t i = 0; i < dayCount; ++i) {
    const int32_t bucket = bucketOf(range, days[i].day);
    if (bucket >= firstBucket && bucket < firstBucket + count) out[bucket - firstBucket] += days[i].activeSec;
  }
}

}  // namespace stats
