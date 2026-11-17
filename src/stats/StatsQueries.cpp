#include "StatsQueries.h"

#include <Arduino.h>
#include <Epub.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <TrustedTime.h>

#include <algorithm>
#include <cstddef>

#include "CrossPointSettings.h"
#include "ReadingStats.h"
#include "StatsLogic.h"
#include "util/TaskWatchdog.h"

namespace stats {

namespace {
bool readExact(HalFile& file, void* out, const size_t size) { return file.read(out, size) == static_cast<int>(size); }

bool ignoreShort() { return SETTINGS.statsIgnoreShortOpens != 0; }

size_t readHalFile(void* ctx, void* buf, const size_t n) {
  if (n == 0) return 0;
  const int read = static_cast<HalFile*>(ctx)->read(buf, n);
  return read > 0 ? static_cast<size_t>(read) : 0;
}

bool hasReading(const ChapterStats& c) {
  return c.activeMs > 0 || c.starts > 0 || c.ends > 0 || c.turnsBack > 0 || c.pauses > 0 || c.lookups > 0;
}

// Log records between watchdog resets while a book's log is replayed.
constexpr uint32_t WATCHDOG_RECORDS = 256;

// Longest TOC title kept per chapter, cut at a UTF-8 character boundary.
constexpr size_t MAX_CHAPTER_TITLE = 48;

// The running session's share of the day totals: true with its day, else (when it counts without
// a wall time) its time added to undatedSec.
bool liveDayShare(DayRecord& day, uint32_t& undatedSec) {
  SessionRecord live{};
  bool hidden = false;
  if (!READING_STATS.liveSession(live, hidden) || hidden || !countsInTotals(live, ignoreShort())) return false;
  if (!countsInCalendar(live, ignoreShort(), hidden)) {
    undatedSec += live.activeSec;
    return false;
  }
  day = DayRecord{};
  day.day = localDay(live.startUtc, live.tzOffsetMin);
  day.activeSec = live.activeSec;
  day.sessions = 1;
  return true;
}

bool openDays(HalFile& file, DaysHeader& header) {
  return Storage.exists(DAYS_PATH) && Storage.openFileForRead("STATS", DAYS_PATH, file) &&
         readExact(file, &header, sizeof(header)) &&
         headerMatches(header.file, MAGIC_DAYS, DAYS_VERSION, sizeof(DaysHeader), sizeof(DayRecord));
}

void appendTitle(std::string& pool, const std::string& title) {
  size_t len = std::min(title.size(), MAX_CHAPTER_TITLE);
  while (len > 0 && len < title.size() && (static_cast<uint8_t>(title[len]) & 0xC0) == 0x80) --len;
  pool.append(title, 0, len);
  pool.push_back('\0');
}

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
  {
    HalFile file;
    DaysHeader header{};
    // 16 bytes per reading day (and one for the running session); the screen keeps them while it is open.
    if (openDays(file, header)) out.days.reserve((file.size() - sizeof(DaysHeader)) / sizeof(DayRecord) + 1);
  }
  forEachDay([](void* days, const DayRecord& day) { static_cast<std::vector<DayRecord>*>(days)->push_back(day); },
             &out.days, out.unknownTimeSec);
  return true;
}

void forEachDay(const DayFn fn, void* ctx, uint32_t& unknownTimeSec, const bool* stop) {
  unknownTimeSec = 0;
  DayRecord live{};
  bool livePending = liveDayShare(live, unknownTimeSec);
  HalFile file;
  DaysHeader header{};
  if (openDays(file, header)) {
    unknownTimeSec += header.unknownTimeActiveSec;
    DayRecord day{};
    for (uint32_t n = 1; readExact(file, &day, sizeof(day)); ++n) {
      if (livePending && live.day <= day.day) {
        if (live.day == day.day) {
          mergeDay(day, live);
        } else {
          fn(ctx, live);
        }
        livePending = false;
      }
      fn(ctx, day);
      if (stop && *stop) return;
      if (n % WATCHDOG_RECORDS == 0) resetTaskWatchdogIfSubscribed();
    }
  }
  if (livePending) fn(ctx, live);
}

void forEachSession(const AnySessionFn fn, void* ctx, const bool* stop) {
  std::vector<uint8_t> hidden;
  READING_STATS.loadHiddenFlags(hidden);
  {
    HalFile file;
    FileHeader header{};
    if (Storage.exists(SESSIONS_PATH) && Storage.openFileForRead("STATS", SESSIONS_PATH, file) &&
        readExact(file, &header, sizeof(header)) &&
        headerMatches(header, MAGIC_SESSIONS, SESSIONS_VERSION, sizeof(FileHeader), sizeof(SessionRecord))) {
      SessionRecord record{};
      for (uint32_t n = 1; readExact(file, &record, sizeof(record)); ++n) {
        if (n % WATCHDOG_RECORDS == 0) resetTaskWatchdogIfSubscribed();
        if (record.bookIndex >= hidden.size()) continue;
        fn(ctx, record, false, hidden[record.bookIndex] != 0);
        if (stop && *stop) return;
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

namespace {
// Every book's counted time, last dated start and style (the Books list pass); index = book.
void collectBooks(const std::vector<uint8_t>& hidden, std::vector<BookListEntry>& books) {
  const size_t count = hidden.size();
  // While loading: 16 bytes per book here, 16 in the style counters, 1 for the finished flag.
  books.assign(count, BookListEntry{});
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
    books[i].style = classifyStyle(styles[i], finished[i] != 0, known, todayDay);
    books[i].lastDay = styles[i].lastDay;
  }
}
}  // namespace

bool loadBookList(const bool hiddenBooks, std::vector<BookListEntry>& out) {
  out.clear();
  std::vector<uint8_t> hidden;
  READING_STATS.loadHiddenFlags(hidden);
  std::vector<BookListEntry> books;
  collectBooks(hidden, books);
  for (size_t i = 0; i < books.size(); ++i) {
    if (hiddenBooks ? hidden[i] == 0 : (hidden[i] != 0 || !books[i].counted)) continue;
    out.push_back(books[i]);
  }
  std::sort(out.begin(), out.end(), [](const BookListEntry& a, const BookListEntry& b) {
    return a.lastUtc != b.lastUtc ? a.lastUtc > b.lastUtc : a.index > b.index;
  });
  return true;
}

bool BookNames::open() {
  if (!Storage.exists(REGISTRY_PATH) || !Storage.openFileForRead("STATS", REGISTRY_PATH, file)) return false;
  const size_t size = file.size();
  count = size < sizeof(RegistryHeader) ? 0 : static_cast<int>((size - sizeof(RegistryHeader)) / sizeof(BookRecord));
  return true;
}

void BookNames::load(const uint16_t index) {
  if (index == loaded) return;
  loaded = index;
  // Title and author are adjacent in the record: one read.
  static_assert(offsetof(BookRecord, author) == offsetof(BookRecord, title) + sizeof(BookRecord::title));
  static_assert(sizeof(Text) == sizeof(BookRecord::title) + sizeof(BookRecord::author));
  if (index >= count || !file.seek(sizeof(RegistryHeader) + index * sizeof(BookRecord) + offsetof(BookRecord, title)) ||
      !readExact(file, &text, sizeof(text))) {
    text.title[0] = text.author[0] = '\0';
    return;
  }
  text.title[sizeof(text.title) - 1] = '\0';
  text.author[sizeof(text.author) - 1] = '\0';
}

bool forEachBook(const BookRowFn fn, void* ctx) {
  std::vector<uint8_t> hidden;
  if (!READING_STATS.loadHiddenFlags(hidden)) return false;
  std::vector<BookListEntry> books;
  collectBooks(hidden, books);

  // The open registry and one book's names (about 200 bytes).
  auto names = makeUniqueNoThrow<BookNames>();
  if (!names) {
    LOG_ERR("STATS", "OOM: book names (%u bytes)", static_cast<unsigned>(sizeof(BookNames)));
    return false;
  }
  if (!names->open()) return false;
  HalFile totalsFile;
  FileHeader header{};
  const bool haveTotals = Storage.exists(TOTALS_PATH) && Storage.openFileForRead("STATS", TOTALS_PATH, totalsFile) &&
                          readExact(totalsFile, &header, sizeof(header)) &&
                          headerMatches(header, MAGIC_TOTALS, TOTALS_VERSION, sizeof(FileHeader), sizeof(BookTotals));
  for (size_t i = 0; i < books.size(); ++i) {
    if (!books[i].counted && hidden[i] == 0) continue;
    BookTotals totals{};
    if (!haveTotals || !totalsFile.seek(sizeof(FileHeader) + i * sizeof(BookTotals)) ||
        !readExact(totalsFile, &totals, sizeof(totals))) {
      totals = BookTotals{};
    }
    names->load(static_cast<uint16_t>(i));
    const BookExport row{static_cast<uint16_t>(i),
                         names->title(),
                         names->author(),
                         totals,
                         hidden[i] != 0,
                         books[i].style,
                         localTzOffsetMin(totals.firstUtc),
                         localTzOffsetMin(totals.lastUtc),
                         localTzOffsetMin(totals.finishedUtc)};
    if (!fn(ctx, row)) break;
    resetTaskWatchdogIfSubscribed();
  }
  return true;
}

bool loadBookDetail(const uint16_t index, BookDetail& out, const BookDetailLoad load) {
  out = BookDetail{};
  std::vector<uint8_t> hidden;
  READING_STATS.loadHiddenFlags(hidden);
  if (index >= hidden.size()) return false;
  out.hidden = hidden[index] != 0;

  struct Context {
    BookDetail* out;
    uint16_t index;
    bool ignoreShort;
    bool progress;
    bool rows;
    size_t next;  // ring position once MAX_BOOK_SESSIONS rows are kept
  } context{&out, index, ignoreShort(), load != BookDetailLoad::Totals, load == BookDetailLoad::Full, 0};
  if (context.progress) out.progress.reserve(MAX_PROGRESS_POINTS + 1);
  forEachSession(
      [](void* c, const SessionRecord& s, const bool live, bool) {
        auto& x = *static_cast<Context*>(c);
        if (s.bookIndex != x.index ||
            !addBookSession(x.out->agg, s, x.ignoreShort, x.progress ? &x.out->progress : nullptr)) {
          return;
        }
        const uint32_t late = lateNightSec(s);
        if (late > 0) {
          ++x.out->lateSessions;
          x.out->lateSec += late;
        }
        if (!x.rows) return;
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

bool loadRecords(Records& out) {
  out = Records{};
  std::vector<uint8_t> hidden;
  READING_STATS.loadHiddenFlags(hidden);
  // 24 bytes per book while loading: sittings and the fastest book.
  std::vector<RecordsBook> books(hidden.size());
  struct Context {
    Records* out;
    std::vector<RecordsBook>* books;
    bool ignoreShort;
  } context{&out, &books, ignoreShort()};
  forEachSession(
      [](void* c, const SessionRecord& session, bool, const bool isHidden) {
        const auto& x = *static_cast<Context*>(c);
        if (!isHidden && countsInTotals(session, x.ignoreShort)) addRecordSession(*x.out, *x.books, session);
      },
      &context);
  DayTotals days;
  loadDayTotals(days);
  finishRecords(out, books, days.days.data(), days.days.size());
  return true;
}

int loadReadingYears(int* out, const int maxYears) {
  if (maxYears <= 0) return 0;
  // Ascending while the days stream past, the newest maxYears kept; reversed at the end.
  struct Context {
    int* years;
    int max;
    int count;
  } context{out, maxYears, 0};
  uint32_t undatedSec = 0;
  forEachDay(
      [](void* c, const DayRecord& day) {
        auto& x = *static_cast<Context*>(c);
        const int year = static_cast<int>(yearOf(day.day));
        if (x.count > 0 && x.years[x.count - 1] == year) return;
        if (x.count == x.max) {
          std::copy(x.years + 1, x.years + x.max, x.years);
          --x.count;
        }
        x.years[x.count++] = year;
      },
      &context, undatedSec);
  int count = context.count;
  std::reverse(out, out + count);
  bool known = false;
  const int32_t todayDay = today(known);
  if (count == 0 && known) out[count++] = static_cast<int>(yearOf(todayDay));
  return count;
}

bool loadYearReview(const int year, YearReview& out) {
  out = YearReview{};
  out.year = year;
  std::vector<uint8_t> hidden;
  READING_STATS.loadHiddenFlags(hidden);
  // 24 bytes per book while loading: the year's time and the book's style.
  std::vector<YearBook> books(hidden.size());
  struct Context {
    YearReview* out;
    std::vector<YearBook>* books;
    bool ignoreShort;
  } context{&out, &books, ignoreShort()};
  forEachSession(
      [](void* c, const SessionRecord& session, bool, const bool isHidden) {
        const auto& x = *static_cast<Context*>(c);
        addYearSession(*x.out, *x.books, session, x.ignoreShort, isHidden);
      },
      &context);
  DayTotals days;
  loadDayTotals(days);
  bool known = false;
  const int32_t todayDay = today(known);
  finishYearReview(out, books, days.days.data(), days.days.size(), known, todayDay);
  return true;
}

bool loadSpeedTrends(SpeedTrends& out) {
  out = SpeedTrends{};
  bool known = false;
  int32_t lastDay = today(known);
  if (!known) {
    DayTotals days;
    loadDayTotals(days);
    lastDay = days.days.empty() ? 0 : days.days.back().day;
  }
  out.firstMonth = monthIndex(lastDay) - (SPEED_MONTHS - 1);
  forEachCalendarSession(
      [](void* c, const SessionRecord& session, bool) { addSpeedSession(*static_cast<SpeedTrends*>(c), session); },
      &out);
  return true;
}

bool loadBookHooks(const uint16_t index, BookHooks& out) {
  const unsigned long startMs = millis();
  out.totals = HookTotals{};
  out.chapters.reset();
  out.count = 0;
  out.titles.clear();
  out.titleAt.clear();
  // The running session's buffered page changes go to its log first.
  READING_STATS.flushPending();

  struct BookModel {
    BookRecord book;
    RegistryHeader header;
  };
  auto model = makeUniqueNoThrow<BookModel>();
  auto replay = makeUniqueNoThrow<ChapterReplay>();
  if (!model || !replay) {
    LOG_ERR("STATS", "OOM: chapter replay (%u bytes)",
            static_cast<unsigned>(sizeof(BookModel) + sizeof(ChapterReplay)));
    return false;
  }
  BookRecord& book = model->book;
  HalFile log;
  if (!READING_STATS.bookRecord(index, book, model->header) || !ReadingStats::openBookLog(book.docId, log)) return true;

  // Pass 1: which sessions count and how many spine items.
  replay->beginCount(chooseRate(book.rateHist, book.rateSamples, model->header.globalHist, model->header.globalSamples),
                     book.typicalPageChars, 0, book.rateSamples >= MIN_MODEL_SAMPLES, ignoreShort());
  uint8_t buf[RAW_MAX_KNOWN_SIZE];
  size_t size = 0;
  {
    LogScanner scanner(&readHalFile, &log, sizeof(FileHeader));
    for (uint32_t n = 1; scanner.next(buf, size); ++n) {
      replay->count(buf, size);
      if (n % WATCHDOG_RECORDS == 0) resetTaskWatchdogIfSubscribed();
    }
  }
  replay->finishCount();
  const size_t spines = replay->chapterCount();
  if (spines == 0) return true;

  // Each spine item's TOC entry, from the book's spine/TOC cache when it is there.
  auto toc = makeUniqueNoThrow<int16_t[]>(spines);
  auto chapters = makeUniqueNoThrow<ChapterStats[]>(spines);
  if (!toc || !chapters) {
    LOG_ERR("STATS", "OOM: chapters (%u bytes)", static_cast<unsigned>(spines * (sizeof(ChapterStats) + 2)));
    return false;
  }
  std::fill(toc.get(), toc.get() + spines, static_cast<int16_t>(-1));
  book.path[sizeof(book.path) - 1] = '\0';
  const unsigned long tocStartMs = millis();
  std::unique_ptr<BookMetadataCache> meta;
  if (Storage.exists(book.path)) {
    meta = makeUniqueNoThrow<BookMetadataCache>(Epub::cachePathFor(book.path, "/.crosspoint"));
    if (meta && !meta->load()) meta.reset();
  }
  const size_t spineItems = meta ? std::min<size_t>(spines, static_cast<size_t>(meta->getSpineCount())) : 0;
  for (size_t i = 0; i < spineItems; ++i) toc[i] = meta->getSpineEntry(static_cast<int>(i)).tocIndex;
  LOG_INF("STATS", "TOC of book %u: %s, %lu ms", static_cast<unsigned>(index), meta ? "loaded" : "none",
          millis() - tocStartMs);

  // Pass 2: the chapters.
  replay->beginChapters(chapters.get(), spines, meta ? toc.get() : nullptr);
  if (!log.seek(sizeof(FileHeader))) return false;
  {
    LogScanner scanner(&readHalFile, &log, sizeof(FileHeader));
    for (uint32_t n = 1; scanner.next(buf, size); ++n) {
      replay->feed(buf, size);
      if (n % WATCHDOG_RECORDS == 0) resetTaskWatchdogIfSubscribed();
    }
  }
  replay->finish();
  log.close();
  out.totals = replay->totals();
  replay.reset();

  // Spine items of one TOC entry become one chapter; chapters without reading are left out. A
  // row's TOC index goes to toc[row] (row <= spine item, so the map is read before it is reused).
  size_t count = 0;
  for (size_t i = 0; i < spines; ++i) {
    const int16_t tocIndex = toc[i];
    if (count > 0 && tocIndex >= 0 && tocIndex == toc[count - 1]) {
      mergeChapter(chapters[count - 1], chapters[i]);
      continue;
    }
    if (count > 0 && !hasReading(chapters[count - 1])) --count;
    chapters[count] = chapters[i];
    toc[count++] = tocIndex;
  }
  if (count > 0 && !hasReading(chapters[count - 1])) --count;

  out.titleAt.reserve(count);
  out.titles.reserve(count * 24);
  for (size_t i = 0; i < count; ++i) {
    out.titleAt.push_back(static_cast<uint16_t>(out.titles.size()));
    if (meta && toc[i] >= 0) {
      const BookMetadataCache::TocEntry entry = meta->getTocEntry(toc[i]);
      appendTitle(out.titles, entry.title);
    } else {
      out.titles.push_back('\0');
    }
  }
  out.chapters = std::move(chapters);
  out.count = count;
  LOG_INF("STATS", "Chapters of book %u: %u in %lu ms", static_cast<unsigned>(index), static_cast<unsigned>(count),
          millis() - startMs);
  return true;
}
}  // namespace stats
