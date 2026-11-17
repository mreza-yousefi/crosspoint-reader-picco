#include "StatsQueries.h"

#include <HalStorage.h>
#include <TrustedTime.h>

#include <algorithm>

#include "CrossPointSettings.h"
#include "ReadingStats.h"
#include "StatsLogic.h"

namespace stats {

namespace {
bool readExact(HalFile& file, void* out, const size_t size) { return file.read(out, size) == static_cast<int>(size); }

bool ignoreShort() { return SETTINGS.statsIgnoreShortOpens != 0; }

}  // namespace

int32_t today(bool& known) {
  const int64_t now = trustedtime::trustedNow();
  known = now > 0;
  if (!known) return 0;
  const auto utc = static_cast<uint32_t>(now);
  return localDay(utc, localTzOffsetMin(utc));
}

bool loadDayTotals(DayTotals& out) {
  out.days.clear();
  out.unknownTimeSec = 0;
  {
    HalFile file;
    DaysHeader header{};
    if (Storage.exists(DAYS_PATH) && Storage.openFileForRead("STATS", DAYS_PATH, file) &&
        readExact(file, &header, sizeof(header)) &&
        headerMatches(header.file, MAGIC_DAYS, DAYS_VERSION, sizeof(DaysHeader), sizeof(DayRecord))) {
      out.unknownTimeSec = header.unknownTimeActiveSec;
      // 16 bytes per reading day; the screen keeps them while it is open.
      out.days.reserve((file.size() - sizeof(DaysHeader)) / sizeof(DayRecord) + 1);
      DayRecord day{};
      while (readExact(file, &day, sizeof(day))) out.days.push_back(day);
    }
  }

  SessionRecord live{};
  bool hidden = false;
  if (READING_STATS.liveSession(live, hidden) && countsInTotals(live, ignoreShort()) && !hidden) {
    if (countsInCalendar(live, ignoreShort(), hidden)) {
      DayRecord add{};
      add.day = localDay(live.startUtc, live.tzOffsetMin);
      add.activeSec = live.activeSec;
      add.sessions = 1;
      addDay(out.days, add);
    } else {
      out.unknownTimeSec += live.activeSec;
    }
  }
  return true;
}

void forEachSession(const AnySessionFn fn, void* ctx) {
  std::vector<uint8_t> hidden;
  READING_STATS.loadHiddenFlags(hidden);
  {
    HalFile file;
    FileHeader header{};
    if (Storage.exists(SESSIONS_PATH) && Storage.openFileForRead("STATS", SESSIONS_PATH, file) &&
        readExact(file, &header, sizeof(header)) &&
        headerMatches(header, MAGIC_SESSIONS, SESSIONS_VERSION, sizeof(FileHeader), sizeof(SessionRecord))) {
      SessionRecord record{};
      while (readExact(file, &record, sizeof(record))) {
        if (record.bookIndex >= hidden.size()) continue;
        fn(ctx, record, false, hidden[record.bookIndex] != 0);
      }
    }
  }
  SessionRecord live{};
  bool liveHidden = false;
  if (READING_STATS.liveSession(live, liveHidden)) fn(ctx, live, true, liveHidden);
}

void forEachCalendarSession(const SessionFn fn, void* ctx) {
  struct Context {
    SessionFn fn;
    void* ctx;
    bool ignoreShort;
  } context{fn, ctx, ignoreShort()};
  forEachSession(
      [](void* c, const SessionRecord& session, const bool live, const bool hidden) {
        const auto& x = *static_cast<Context*>(c);
        if (countsInCalendar(session, x.ignoreShort, hidden)) x.fn(x.ctx, session, live);
      },
      &context);
}

bool loadDaySessions(const int32_t day, std::vector<DaySession>& out) {
  out.clear();
  struct Context {
    int32_t day;
    std::vector<DaySession>* out;
  } context{day, &out};
  forEachCalendarSession(
      [](void* ctx, const SessionRecord& session, const bool live) {
        auto& c = *static_cast<Context*>(ctx);
        if (localDay(session.startUtc, session.tzOffsetMin) == c.day) c.out->push_back({session, live});
      },
      &context);
  return true;
}

int countFinishedInYear(const int year) {
  std::vector<uint8_t> hidden;
  READING_STATS.loadHiddenFlags(hidden);
  HalFile file;
  FileHeader header{};
  if (!Storage.exists(TOTALS_PATH) || !Storage.openFileForRead("STATS", TOTALS_PATH, file) ||
      !readExact(file, &header, sizeof(header)) ||
      !headerMatches(header, MAGIC_TOTALS, TOTALS_VERSION, sizeof(FileHeader), sizeof(BookTotals))) {
    return 0;
  }
  int finished = 0;
  BookTotals totals{};
  for (size_t i = 0; readExact(file, &totals, sizeof(totals)); ++i) {
    if (i < hidden.size() && hidden[i]) continue;
    if (totals.finishedUtc <= 1) continue;
    if (yearOf(localDay(totals.finishedUtc, localTzOffsetMin(totals.finishedUtc))) == year) ++finished;
  }
  return finished;
}

bool loadBookList(const bool hiddenBooks, std::vector<BookListEntry>& out) {
  out.clear();
  std::vector<uint8_t> hidden;
  READING_STATS.loadHiddenFlags(hidden);
  const size_t count = hidden.size();
  // While loading: 16 bytes per book here, 16 in the style counters, 1 for the finished flag.
  std::vector<BookListEntry> books(count);
  std::vector<StyleAcc> styles(count);
  std::vector<uint8_t> finished(count, 0);
  for (size_t i = 0; i < count; ++i) books[i].index = static_cast<uint16_t>(i);

  struct Context {
    std::vector<BookListEntry>* books;
    std::vector<StyleAcc>* styles;
    std::vector<uint8_t>* finished;
    bool ignoreShort;
  } context{&books, &styles, &finished, ignoreShort()};
  forEachSession(
      [](void* c, const SessionRecord& session, bool, bool) {
        auto& x = *static_cast<Context*>(c);
        if (session.bookIndex >= x.books->size()) return;
        if (finishesBook(session)) (*x.finished)[session.bookIndex] = 1;
        if (!countsInTotals(session, x.ignoreShort)) return;
        BookListEntry& book = (*x.books)[session.bookIndex];
        book.counted = true;
        book.activeSec += session.activeSec;
        if (hasWallTime(session)) book.lastUtc = std::max(book.lastUtc, session.startUtc);
        addStyleSession((*x.styles)[session.bookIndex], session);
      },
      &context);

  bool known = false;
  const int32_t todayDay = today(known);
  for (size_t i = 0; i < count; ++i) {
    BookListEntry& book = books[i];
    if (hiddenBooks ? hidden[i] == 0 : (hidden[i] != 0 || !book.counted)) continue;
    book.style = classifyStyle(styles[i], finished[i] != 0, known, todayDay);
    book.lastDay = styles[i].lastDay;
    out.push_back(book);
  }
  std::sort(out.begin(), out.end(), [](const BookListEntry& a, const BookListEntry& b) {
    return a.lastUtc != b.lastUtc ? a.lastUtc > b.lastUtc : a.index > b.index;
  });
  return true;
}

bool loadBookDetail(const uint16_t index, BookDetail& out, const bool full) {
  out = BookDetail{};
  std::vector<uint8_t> hidden;
  READING_STATS.loadHiddenFlags(hidden);
  if (index >= hidden.size()) return false;
  out.hidden = hidden[index] != 0;

  struct Context {
    BookDetail* out;
    uint16_t index;
    bool ignoreShort;
    bool full;
    size_t next;  // ring position once MAX_BOOK_SESSIONS rows are kept
  } context{&out, index, ignoreShort(), full, 0};
  if (full) out.progress.reserve(MAX_PROGRESS_POINTS + 1);
  forEachSession(
      [](void* c, const SessionRecord& s, const bool live, bool) {
        auto& x = *static_cast<Context*>(c);
        if (s.bookIndex != x.index ||
            !addBookSession(x.out->agg, s, x.ignoreShort, x.full ? &x.out->progress : nullptr) || !x.full) {
          return;
        }
        const BookSessionRow row{s.startUtc,       s.durationSec,  s.activeSec, s.dictSec, s.tzOffsetMin,
                                 s.startPercentBp, s.endPercentBp, s.lookups,   s.flags,   live};
        auto& rows = x.out->sessions;
        if (rows.size() < MAX_BOOK_SESSIONS) {
          rows.push_back(row);
        } else {
          rows[x.next] = row;
          x.next = (x.next + 1) % MAX_BOOK_SESSIONS;
        }
      },
      &context);
  auto& rows = out.sessions;
  std::rotate(rows.begin(), rows.begin() + static_cast<std::ptrdiff_t>(context.next), rows.end());
  std::reverse(rows.begin(), rows.end());

  bool known = false;
  const int32_t todayDay = today(known);
  out.style = classifyStyle(out.agg.style, out.agg.totals.finishedUtc != 0, known, todayDay);
  return true;
}

void loadHourGrid(const int32_t fromDay, uint32_t* grid) {
  std::fill(grid, grid + GRID_CELLS, 0u);
  struct Context {
    int32_t fromDay;
    uint32_t* grid;
  } context{fromDay, grid};
  forEachCalendarSession(
      [](void* c, const SessionRecord& session, bool) {
        const auto& x = *static_cast<Context*>(c);
        if (localDay(session.startUtc, session.tzOffsetMin) >= x.fromDay) addToHourGrid(x.grid, session);
      },
      &context);
}

}  // namespace stats
