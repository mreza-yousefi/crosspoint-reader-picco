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
  inDict = false;
  lastMs = 0;
  pausedAtMs = 0;
  dictSinceMs = 0;
  activeMs = 0;
  dictMs = 0;
}

uint32_t ActiveClock::capMs(const uint16_t chars) const { return dwellCapSec(rate, chars) * 1000; }

void ActiveClock::start(const uint32_t nowMs, const PagePos& pos) {
  started = true;
  paused = false;
  inDict = false;
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
  dictionaryClose(nowMs);
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
  dictionaryClose(nowMs);
  paused = true;
  pausedAtMs = nowMs;
}

void ActiveClock::resume(const uint32_t nowMs) {
  if (!paused) return;
  paused = false;
  lastMs += nowMs - pausedAtMs;  // the dwell goes on as if the pause had not happened
}

void ActiveClock::dictionaryOpen(const uint32_t nowMs) {
  if (!started || inDict) return;
  resume(nowMs);  // a lookup started from the menu is reading again
  inDict = true;
  dictSinceMs = nowMs;
}

void ActiveClock::dictionaryClose(const uint32_t nowMs) {
  if (!inDict) return;
  inDict = false;
  dictMs += std::min(nowMs - dictSinceMs, DICT_MAX_SEC * 1000);
  lastMs += nowMs - dictSinceMs;  // the page dwell goes on as if the lookup had not happened
}

uint32_t ActiveClock::openDwellMs(const uint32_t nowMs) const {
  if (!started) return 0;
  const uint32_t end = paused ? pausedAtMs : inDict ? dictSinceMs : nowMs;
  return std::min(end - lastMs, capMs(typicalChars));
}

uint32_t ActiveClock::typicalDwellMs() const {
  if (!(rate > 0.0f)) return IDLE_TRIM_DEFAULT_SEC * 1000;
  return static_cast<uint32_t>(rate * static_cast<float>(typicalChars));  // rate is ms per char
}

uint32_t ActiveClock::dictionaryMsAt(const uint32_t nowMs, const bool trimIdle) const {
  if (!inDict) return dictMs;
  const uint32_t open = std::min(nowMs - dictSinceMs, DICT_MAX_SEC * 1000);
  return dictMs + (trimIdle ? std::min(open, typicalDwellMs()) : open);
}

uint32_t ActiveClock::activeMsAt(const uint32_t nowMs, const bool trimIdle) const {
  uint32_t page = openDwellMs(nowMs);
  // The open stretch is the dictionary when it is up, else the page dwell. A paused dwell
  // ended before the pause: it was reading, not idle time.
  if (trimIdle && !inDict && !paused) page = std::min(page, typicalDwellMs());
  return activeMs + page + dictionaryMsAt(nowMs, trimIdle);
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

void LogReplay::begin(const uint16_t book, const float bookRate, const uint16_t typicalPageChars, const uint32_t skip,
                      const bool trim) {
  bookIndex = book;
  rate = bookRate;
  typicalChars = typicalPageChars;
  skipSeq = skip;
  idleTrim = trim;
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
    rec.dictSec = end->dictSec;
    if (idleTrim && end->reason == END_AUTO_SLEEP && !(end->flags & SESSION_IDLE_TRIMMED)) {
      // Written before the idle trim: take the trimmed part of the last stretch off.
      const uint32_t endMs = end->durationMs;
      const uint64_t trimMs = clock.activeMsAt(endMs) - clock.activeMsAt(endMs, true);
      const uint64_t dictTrimMs = clock.dictionaryMsAt(endMs) - clock.dictionaryMsAt(endMs, true);
      const uint64_t activeMs = static_cast<uint64_t>(rec.activeSec) * 1000;
      const uint64_t dictMsTotal = static_cast<uint64_t>(rec.dictSec) * 1000;
      rec.activeSec = static_cast<uint32_t>(activeMs > trimMs ? (activeMs - trimMs) / 1000 : 0);
      rec.dictSec = static_cast<uint32_t>(dictMsTotal > dictTrimMs ? (dictMsTotal - dictTrimMs) / 1000 : 0);
      rec.flags |= SESSION_IDLE_TRIMMED;
      if (isShortOpen(rec.activeSec, rec.turnsFwd + rec.turnsBack + rec.jumps)) rec.flags |= SESSION_SHORT;
    }
    if (rec.startUtc == 0 && end->endUtc > rec.durationSec) {
      // The clock became trusted during the session: date it from the end.
      rec.startUtc = end->endUtc - rec.durationSec;
      rec.tzOffsetMin = end->tzOffsetMin;
      rec.flags |= SESSION_WALL_KNOWN;
    }
  } else {
    rec.durationSec = lastMs / 1000;
    rec.activeSec = clock.activeMsAt(lastMs) / 1000;
    rec.dictSec = clock.dictionaryMsAt(lastMs) / 1000;
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
      rec.paperPagesTotal = start.paperPagesTotal;
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
        case EVENT_DICT_OPEN:
          clock.dictionaryOpen(event.tMs);
          break;
        case EVENT_DICT_CLOSE:
          clock.dictionaryClose(event.tMs);
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

void bucketDays(const Range range, const int32_t bucket, int32_t& firstDay, int32_t& lastDay) {
  switch (range) {
    case Range::Day:
      firstDay = lastDay = bucket;
      return;
    case Range::Week:
      firstDay = weekStartDay(bucket);
      lastDay = firstDay + 6;
      return;
    case Range::Month: {
      const int year = floorDiv(bucket, 12);
      const auto month = static_cast<unsigned>(bucket - year * 12) + 1;
      firstDay = daysFromCivil(year, month, 1);
      lastDay = month == 12 ? daysFromCivil(year + 1, 1, 1) - 1 : daysFromCivil(year, month + 1, 1) - 1;
      return;
    }
    case Range::Year:
      firstDay = daysFromCivil(bucket, 1, 1);
      lastDay = daysFromCivil(bucket + 1, 1, 1) - 1;
      return;
  }
  firstDay = lastDay = bucket;
}

void addToHourGrid(uint32_t* grid, const SessionRecord& session) {
  if (!hasWallTime(session) || session.activeSec == 0) return;
  const int64_t start = static_cast<int64_t>(session.startUtc) + static_cast<int64_t>(session.tzOffsetMin) * 60;
  // A day or longer is not one sitting: all of it goes to the start hour.
  const int64_t length = session.durationSec == 0 || session.durationSec > 86400 ? 1 : session.durationSec;
  const int64_t end = start + length;
  uint32_t left = session.activeSec;
  for (int64_t t = start; t < end && left > 0;) {
    const int64_t next = std::min<int64_t>((static_cast<int64_t>(floorDiv(t, 3600)) + 1) * 3600, end);
    const uint32_t share =
        next == end ? left : std::min<uint32_t>(left, static_cast<uint32_t>(session.activeSec * (next - t) / length));
    const int32_t day = floorDiv(t, 86400);
    const int hour = static_cast<int>((t - static_cast<int64_t>(day) * 86400) / 3600);
    grid[weekday(day) * 24 + hour] += share;
    left -= share;
    t = next;
  }
}

int gridShade(const uint32_t value, const uint32_t max) {
  if (value == 0 || max == 0) return 0;
  return static_cast<int>(std::min<uint64_t>(3, (3ull * value + max - 1) / max));
}

void addStyleSession(StyleAcc& acc, const SessionRecord& session) {
  if (!hasWallTime(session)) return;
  const int32_t day = localDay(session.startUtc, session.tzOffsetMin);
  if (acc.sessions == 0) {
    acc.firstDay = acc.lastDay = day;
    acc.days = 1;
  } else if (day != acc.lastDay) {
    acc.days = addSat16(acc.days, 1);
    acc.firstDay = std::min(acc.firstDay, day);
    acc.lastDay = std::max(acc.lastDay, day);
  }
  acc.sessions = addSat16(acc.sessions, 1);
  acc.activeSec += session.activeSec;
}

ReadingStyle classifyStyle(const StyleAcc& acc, const bool finished, const bool todayKnown, const int32_t today) {
  if (acc.sessions < STYLE_MIN_SESSIONS || acc.days == 0) return ReadingStyle::None;
  if (!finished && todayKnown && today - acc.lastDay > STALLED_DAYS) return ReadingStyle::Stalled;
  const int64_t span = static_cast<int64_t>(acc.lastDay) - acc.firstDay + 1;
  const bool dense = static_cast<int64_t>(acc.days) * 2 >= span;
  if (dense && acc.activeSec / acc.days >= BINGE_DAY_SEC) return ReadingStyle::Binge;
  if (acc.activeSec / acc.sessions < SNACK_SESSION_SEC) return ReadingStyle::Snacking;
  return dense ? ReadingStyle::Steady : ReadingStyle::Intermittent;
}

bool addBookSession(BookAggregate& agg, const SessionRecord& session, const bool ignoreShort,
                    std::vector<ProgressPoint>* points) {
  const bool wasFinished = agg.totals.finishedUtc != 0;
  const bool counted = applySession(agg.totals, false, session, ignoreShort).counted;
  if (!wasFinished && agg.totals.finishedUtc > 1) {
    agg.finishedDay = localDay(agg.totals.finishedUtc, session.tzOffsetMin);
    agg.hasFinishedDay = true;
  }
  if (!counted) return false;
  agg.lookups += session.lookups;
  agg.dictSec += session.dictSec;
  if (!agg.hasFirst) {
    agg.firstPercentBp = session.startPercentBp;
    agg.hasFirst = true;
  }
  if (session.endPaperPage > 0) agg.lastPaperPage = session.endPaperPage;
  if (session.paperPagesTotal > 0) agg.paperPagesTotal = session.paperPagesTotal;
  addStyleSession(agg.style, session);
  if (points && hasWallTime(session)) {
    const ProgressPoint point{localDay(session.startUtc, session.tzOffsetMin), session.endPercentBp};
    const auto it = std::lower_bound(points->begin(), points->end(), point.day,
                                     [](const ProgressPoint& p, const int32_t day) { return p.day < day; });
    if (it != points->end() && it->day == point.day) {
      it->percentBp = point.percentBp;
    } else {
      points->insert(it, point);
    }
    if (points->size() > MAX_PROGRESS_POINTS) {
      // Keep the first, every second one after it, and the last.
      auto& p = *points;
      const size_t count = p.size();
      size_t kept = 0;
      for (size_t i = 0; i < count; i += 2) p[kept++] = p[i];
      if (count % 2 == 0) p[kept++] = p[count - 1];
      p.resize(kept);
    }
  }
  return true;
}

uint32_t pagesPerHour(const uint32_t charsRead, const uint32_t learnedMs) {
  constexpr uint32_t MIN_MEASURED_MS = 5 * 60 * 1000;
  if (learnedMs < MIN_MEASURED_MS || charsRead == 0) return 0;
  const uint64_t denominator = static_cast<uint64_t>(learnedMs) * PAPER_PAGE_CHARS;
  return static_cast<uint32_t>((static_cast<uint64_t>(charsRead) * 3600000 + denominator / 2) / denominator);
}

uint32_t secondsToFinish(const BookAggregate& agg) {
  const BookTotals& t = agg.totals;
  const uint16_t paperPagesTotal = agg.paperPagesTotal;
  if (t.finishedUtc != 0) return 0;
  if (paperPagesTotal > 0 && agg.lastPaperPage > 0 && agg.lastPaperPage < paperPagesTotal &&
      pagesPerHour(t.charsRead, t.learnedMs) > 0) {
    const uint64_t charsLeft = static_cast<uint64_t>(paperPagesTotal - agg.lastPaperPage) * PAPER_PAGE_CHARS;
    return static_cast<uint32_t>(charsLeft * t.learnedMs / t.charsRead / 1000);
  }
  constexpr uint16_t MIN_PROGRESS_BP = 100;  // 1 %
  if (agg.hasFirst && t.lastPercentBp < END_OF_BOOK_BP && t.lastPercentBp >= agg.firstPercentBp + MIN_PROGRESS_BP) {
    const uint32_t progressed = t.lastPercentBp - agg.firstPercentBp;
    return static_cast<uint32_t>(static_cast<uint64_t>(END_OF_BOOK_BP - t.lastPercentBp) * t.totalActiveSec /
                                 progressed);
  }
  return 0;
}

int daysToFinish(const BookAggregate& agg) {
  if (!agg.hasFinishedDay || agg.style.sessions == 0 || agg.finishedDay < agg.style.firstDay) return 0;
  return static_cast<int>(agg.finishedDay - agg.style.firstDay) + 1;
}

void progressColumns(const ProgressPoint* points, const size_t count, const int32_t firstDay, const int32_t lastDay,
                     const int columns, uint16_t* out) {
  if (columns <= 0) return;
  const int64_t span = std::max<int64_t>(1, static_cast<int64_t>(lastDay) - firstDay + 1);
  uint16_t value = 0;
  size_t i = 0;
  for (int c = 0; c < columns; ++c) {
    const int64_t columnLast = firstDay + (static_cast<int64_t>(c) + 1) * span / columns - 1;
    while (i < count && points[i].day <= columnLast) value = points[i++].percentBp;
    out[c] = value;
  }
}

}  // namespace stats
