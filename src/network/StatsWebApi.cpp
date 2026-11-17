#include "StatsWebApi.h"

#include <Logging.h>
#include <Memory.h>
#include <WebServer.h>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "ChunkedReply.h"
#include "CrossPointSettings.h"
#include "stats/ReadingStats.h"
#include "stats/StatsExport.h"
#include "stats/StatsInsights.h"
#include "stats/StatsQueries.h"
#include "util/TaskWatchdog.h"

namespace statsweb {

namespace {

using stats::JsonObjectWriter;

// Rows between watchdog resets and checks that the client is still there.
constexpr uint32_t CHECK_ROWS = 32;
// Years listed by the summary (newest first).
constexpr int MAX_YEARS = 32;

class ReplySink final : public stats::TextSink {
 public:
  explicit ReplySink(WebServer& server) : server(server), reply(server) {}
  void write(const char* data, const size_t len) override { reply.write(reinterpret_cast<const uint8_t*>(data), len); }
  void num(const int64_t value) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(value));
    text(buf);
  }
  // "[a,b,...]".
  void nums(const uint32_t* values, const int count) {
    text("[");
    for (int i = 0; i < count; ++i) {
      if (i > 0) text(",");
      num(values[i]);
    }
    text("]");
  }
  // After each row; false once the client has gone, so a long reply stops reading the card.
  bool alive(const uint32_t rows) {
    if (rows % CHECK_ROWS != 0) return true;
    resetTaskWatchdogIfSubscribed();
    return server.client().connected();
  }
  void end() { reply.end(); }

 private:
  WebServer& server;
  ChunkedReply reply;
};

bool ignoreShort() { return SETTINGS.statsIgnoreShortOpens != 0; }

void sendError(WebServer& server, const int code, const char* message) {
  char body[96];
  snprintf(body, sizeof(body), "{\"error\":\"%s\"}", message);
  server.send(code, "application/json", body);
}

void sendOutOfMemory(WebServer& server, const char* what, const size_t bytes) {
  LOG_ERR("WEB", "OOM: stats %s (%u bytes)", what, static_cast<unsigned>(bytes));
  sendError(server, 503, "out of memory");
}

void startJson(WebServer& server) {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
}

void startCsv(WebServer& server, const char* filename) {
  char disposition[64];
  snprintf(disposition, sizeof(disposition), "attachment; filename=\"%s\"", filename);
  server.sendHeader("Content-Disposition", disposition);
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv; charset=utf-8", "");
}

// A whole non-negative decimal number argument (digits only); false when missing, empty or not a
// number.
bool intArg(WebServer& server, const char* name, long& out) {
  if (!server.hasArg(name)) return false;
  const String text = server.arg(name);
  if (text.isEmpty() || text[0] < '0' || text[0] > '9') return false;
  char* end = nullptr;
  out = strtol(text.c_str(), &end, 10);
  return end && *end == '\0';
}

// The book argument (a books.bin index); false after a 400 (missing or not a number) or 404
// (no such book) reply.
bool bookArg(WebServer& server, uint16_t& out) {
  long value = -1;
  if (!intArg(server, "book", value) || value < 0 || value >= stats::NO_BOOK) {
    sendError(server, 400, "book: a book number is required");
    return false;
  }
  std::vector<uint8_t> hidden;  // one byte per book
  READING_STATS.loadHiddenFlags(hidden);
  if (static_cast<size_t>(value) >= hidden.size()) {
    sendError(server, 404, "unknown book");
    return false;
  }
  out = static_cast<uint16_t>(value);
  return true;
}

void writeRecord(JsonObjectWriter& json, ReplySink& out, stats::BookNames& names, const char* name,
                 const stats::RecordRef& ref) {
  if (ref.book == stats::NO_BOOK) return;
  json.key(name);
  JsonObjectWriter record(out);
  record.num("value", ref.value);
  record.num("book", ref.book);
  names.load(ref.book);
  record.str("title", names.title());
  char start[24];
  stats::formatLocalTime(ref.utc, ref.tzOffsetMin, start, sizeof(start));
  record.str("start", start);
  record.end();
}

// [pages per hour, measured seconds].
void writeSpeedCell(ReplySink& out, const stats::SpeedCell& cell) {
  out.text("[");
  out.num(stats::cellPagesPerHour(cell));
  out.text(",");
  out.num(static_cast<int64_t>(cell.learnedMs / 1000));
  out.text("]");
}

}  // namespace

void handleSummary(WebServer& server) {
  READING_STATS.prepareSummaries();
  bool known = false;
  const int32_t clockDay = stats::today(known);
  int years[MAX_YEARS];
  const int yearCount = stats::loadReadingYears(years, MAX_YEARS);

  startJson(server);
  ReplySink out(server);
  struct Context {
    ReplySink* out;
    stats::OverviewAccumulator overview;
    uint32_t rows;
    bool stop;
  } context{&out, stats::OverviewAccumulator(clockDay, known), 0, false};
  JsonObjectWriter json(out);
  json.key("days");
  out.text("[");
  uint32_t undatedSec = 0;
  stats::forEachDay(
      [](void* ctx, const stats::DayRecord& day) {
        auto& x = *static_cast<Context*>(ctx);
        char row[64];
        snprintf(row, sizeof(row), "%s[%ld,%lu,%u,%u,%u]", x.rows > 0 ? "," : "", static_cast<long>(day.day),
                 static_cast<unsigned long>(day.activeSec), day.sessions, day.sittings, day.pagesRead);
        x.out->text(row);
        x.overview.add(day);
        x.stop = !x.out->alive(++x.rows);
      },
      &context, undatedSec, &context.stop);
  if (context.stop) return;  // the client has gone
  out.text("]");

  const stats::Overview o = context.overview.result();
  json.flag("clockKnown", known);
  json.num("today", o.anchor);
  json.flag("ignoreShortOpens", ignoreShort());
  json.num("totalSec", static_cast<int64_t>(o.daysSec) + undatedSec);
  json.num("undatedSec", undatedSec);
  json.num("longestStreak", o.streaks.longest);
  if (known) {
    json.num("todaySec", o.todaySec);
    json.num("weekSec", o.weekSec);
    json.num("monthSec", o.monthSec);
    json.num("yearSec", o.yearSec);
    json.num("currentStreak", o.streaks.current);
    json.num("finishedThisYear", stats::countFinishedInYear(stats::yearOf(clockDay)));
  }
  json.key("years");
  out.text("[");
  for (int i = 0; i < yearCount; ++i) {
    if (i > 0) out.text(",");
    out.num(years[i]);
  }
  out.text("]");
  json.end();
  out.end();
}

void handleGrid(WebServer& server) {
  READING_STATS.prepareSummaries();
  bool known = false;
  const int32_t todayDay = stats::today(known);
  int32_t fromDay = INT32_MIN;  // without a trusted clock every range shows all
  const String range = server.arg("range");
  if (known && range == "30") fromDay = todayDay - 29;
  if (known && range == "365") fromDay = todayDay - 364;
  auto grid = makeUniqueNoThrow<uint32_t[]>(stats::GRID_CELLS);
  if (!grid) {
    sendOutOfMemory(server, "grid", stats::GRID_CELLS * sizeof(uint32_t));
    return;
  }
  stats::loadHourGrid(fromDay, grid.get());

  startJson(server);
  ReplySink out(server);
  JsonObjectWriter json(out);
  json.flag("clockKnown", known);
  json.key("grid");
  out.nums(grid.get(), stats::GRID_CELLS);
  json.end();
  out.end();
}

void handleBooks(WebServer& server) {
  READING_STATS.prepareSummaries();
  startJson(server);
  ReplySink out(server);
  struct Context {
    ReplySink* out;
    uint32_t rows;
  } context{&out, 0};
  out.text("[");
  stats::forEachBook(
      [](void* ctx, const stats::BookExport& book) {
        auto& x = *static_cast<Context*>(ctx);
        if (x.rows > 0) x.out->text(",");
        stats::writeBookJson(*x.out, book);
        return x.out->alive(++x.rows);
      },
      &context);
  out.text("]");
  out.end();
}

void handleBook(WebServer& server) {
  uint16_t index = 0;
  if (!bookArg(server, index)) return;
  READING_STATS.prepareSummaries();
  // The totals and at most 129 progress points of 8 bytes; the session rows are not loaded.
  auto detail = makeUniqueNoThrow<stats::BookDetail>();
  auto names = makeUniqueNoThrow<stats::BookNames>();
  if (!detail || !names) {
    sendOutOfMemory(server, "book", sizeof(stats::BookDetail) + sizeof(stats::BookNames));
    return;
  }
  if (!stats::loadBookDetail(index, *detail, stats::BookDetailLoad::Progress)) {
    sendError(server, 404, "unknown book");
    return;
  }
  names->open();
  names->load(index);
  const stats::BookAggregate& agg = detail->agg;
  const stats::BookTotals& t = agg.totals;

  startJson(server);
  ReplySink out(server);
  JsonObjectWriter json(out);
  json.num("book", index);
  json.str("title", names->title());
  json.str("author", names->author());
  json.flag("hidden", detail->hidden);
  json.str("style", stats::styleKey(detail->style));
  json.num("active", t.totalActiveSec);
  json.num("sessions", t.sessions);
  json.num("sittings", t.sittings);
  if (agg.style.sessions > 0) {
    json.num("firstDay", agg.style.firstDay);
    json.num("lastDay", agg.style.lastDay);
  }
  json.flag("finished", t.finishedUtc != 0);
  if (agg.hasFinishedDay) json.num("finishedDay", agg.finishedDay);
  json.num("pagesPerHour", stats::pagesPerHour(t.charsRead, t.learnedMs));
  json.num("secondsToFinish", t.finishedUtc != 0 ? 0 : stats::secondsToFinish(agg));
  json.num("daysToFinish", stats::daysToFinish(agg));
  json.num("positionBp", std::min<uint16_t>(t.lastPercentBp, stats::END_OF_BOOK_BP));
  json.num("maxBp", t.maxPercentBp);
  json.num("paperPage", agg.lastPaperPage);
  json.num("paperPages", agg.paperPagesTotal);
  json.num("lookups", agg.lookups);
  json.num("lookupsPerHourTenths", stats::lookupsPerHourTenths(agg.lookups, t.totalActiveSec));
  json.num("dictSec", agg.dictSec);
  json.num("lateSessions", detail->lateSessions);
  json.num("lateSec", detail->lateSec);
  json.key("progress");
  out.text("[");
  for (size_t i = 0; i < detail->progress.size(); ++i) {
    const stats::ProgressPoint& p = detail->progress[i];
    char point[32];
    snprintf(point, sizeof(point), "%s[%ld,%u]", i > 0 ? "," : "", static_cast<long>(p.day), p.percentBp);
    out.text(point);
  }
  out.text("]");
  json.end();
  out.end();
}

void handleSessions(WebServer& server) {
  int book = -1;
  if (server.hasArg("book")) {
    uint16_t index = 0;
    if (!bookArg(server, index)) return;
    book = index;
  }
  READING_STATS.prepareSummaries();
  startJson(server);
  ReplySink out(server);
  struct Context {
    ReplySink* out;
    int book;
    bool ignoreShort;
    uint32_t rows;
    bool stop;
  } context{&out, book, ignoreShort(), 0, false};
  out.text("[");
  stats::forEachSession(
      [](void* ctx, const stats::SessionRecord& session, const bool live, const bool hidden) {
        auto& x = *static_cast<Context*>(ctx);
        if (x.book >= 0 && session.bookIndex != x.book) return;
        if (x.rows > 0) x.out->text(",");
        const stats::SessionExport row{session, "", "", hidden, stats::countsInTotals(session, x.ignoreShort), live};
        stats::writeSessionJson(*x.out, row);
        x.stop = !x.out->alive(++x.rows);
      },
      &context, &context.stop);
  out.text("]");
  out.end();
}

void handleHooks(WebServer& server) {
  uint16_t index = 0;
  if (!bookArg(server, index)) return;
  READING_STATS.prepareSummaries();
  stats::BookHooks hooks;
  if (!stats::loadBookHooks(index, hooks)) {
    sendError(server, 503, "could not read the book's log");
    return;
  }
  const stats::ChapterStats* c = hooks.chapters.get();
  const stats::HookHighlights h = stats::pickHighlights(c, hooks.count);
  const int lastStop = stats::lastStopChapter(c, hooks.count, hooks.totals);

  startJson(server);
  ReplySink out(server);
  JsonObjectWriter json(out);
  json.num("book", index);
  json.num("sessions", hooks.totals.sessions);
  json.num("oneMoreSessions", hooks.totals.oneMoreSessions);
  json.num("lastStop", lastStop);
  json.key("highlights");
  {
    JsonObjectWriter highlights(out);
    highlights.num("mostTime", h.mostTime);
    highlights.num("fastest", h.fastest);
    highlights.num("slowest", h.slowest);
    highlights.num("mostStarts", h.mostStarts);
    highlights.num("mostEnds", h.mostEnds);
    highlights.num("mostReread", h.mostReread);
    highlights.num("mostBack", h.mostBack);
    highlights.num("mostPauses", h.mostPauses);
    highlights.num("mostLookups", h.mostLookups);
    highlights.end();
  }
  json.key("chapters");
  out.text("[");
  for (size_t i = 0; i < hooks.count; ++i) {
    if (i > 0) out.text(",");
    JsonObjectWriter chapter(out);
    chapter.str("title", hooks.titles.c_str() + hooks.titleAt[i]);
    chapter.num("spine", c[i].spine);
    chapter.num("activeMs", c[i].activeMs);
    chapter.num("pagesPerHour", stats::pagesPerHour(c[i].charsRead, c[i].learnedMs));
    chapter.num("starts", c[i].starts);
    chapter.num("ends", c[i].ends);
    chapter.num("pauses", c[i].pauses);
    chapter.num("back", c[i].turnsBack);
    chapter.num("oneMore", c[i].oneMore);
    chapter.num("lookups", c[i].lookups);
    chapter.num("rereadChars", c[i].rereadChars);
    chapter.end();
  }
  out.text("]");
  json.end();
  out.end();
}

void handleRecords(WebServer& server) {
  READING_STATS.prepareSummaries();
  auto records = makeUniqueNoThrow<stats::Records>();
  auto names = makeUniqueNoThrow<stats::BookNames>();
  if (!records || !names) {
    sendOutOfMemory(server, "records", sizeof(stats::Records) + sizeof(stats::BookNames));
    return;
  }
  stats::loadRecords(*records);
  names->open();
  const stats::Records& r = *records;

  startJson(server);
  ReplySink out(server);
  JsonObjectWriter json(out);
  writeRecord(json, out, *names, "longestSession", r.longestSession);
  writeRecord(json, out, *names, "longestSitting", r.longestSitting);
  writeRecord(json, out, *names, "mostLookups", r.mostLookups);
  writeRecord(json, out, *names, "latestNight", r.latestNight);
  writeRecord(json, out, *names, "earliestMorning", r.earliestMorning);
  writeRecord(json, out, *names, "fastestBook", r.fastestBook);
  if (r.mostDaySec > 0) {
    json.num("mostDay", r.mostDay);
    json.num("mostDaySec", r.mostDaySec);
  }
  if (r.mostWeekSec > 0) {
    json.num("mostWeekStart", stats::weekStartDay(r.mostWeek));
    json.num("mostWeekSec", r.mostWeekSec);
  }
  json.num("longestStreak", r.longestStreak);
  json.num("lookups", r.lookups);
  json.num("activeSec", r.activeSec);
  json.num("lookupsPerHourTenths", stats::lookupsPerHourTenths(r.lookups, r.activeSec));
  json.num("lateSessions", r.lateSessions);
  json.num("lateSec", r.lateSec);
  json.end();
  out.end();
}

void handleYear(WebServer& server) {
  READING_STATS.prepareSummaries();
  int year = 0;
  if (server.hasArg("year")) {
    long value = 0;
    if (!intArg(server, "year", value) || value < 1970 || value > 9999) {
      sendError(server, 400, "year: 1970-9999");
      return;
    }
    year = static_cast<int>(value);
  } else if (stats::loadReadingYears(&year, 1) == 0) {
    sendError(server, 404, "no reading years");
    return;
  }
  auto review = makeUniqueNoThrow<stats::YearReview>();
  auto names = makeUniqueNoThrow<stats::BookNames>();
  if (!review || !names) {
    sendOutOfMemory(server, "year", sizeof(stats::YearReview) + sizeof(stats::BookNames));
    return;
  }
  stats::loadYearReview(year, *review);
  names->open();
  const stats::YearReview& y = *review;
  uint32_t weekdays[7] = {};
  uint32_t hours[24] = {};
  stats::gridTotals(y.grid, weekdays, hours);

  startJson(server);
  ReplySink out(server);
  JsonObjectWriter json(out);
  json.num("year", y.year);
  json.num("active", y.activeSec);
  json.num("sessions", y.sessions);
  json.num("lookups", y.lookups);
  json.num("lookupsPerHourTenths", stats::lookupsPerHourTenths(y.lookups, y.activeSec));
  json.num("lateSessions", y.lateSessions);
  json.num("lateSec", y.lateSec);
  json.num("longestStreak", y.longestStreak);
  json.num("finished", stats::countFinishedInYear(year));
  json.num("busiestMonth", stats::busiestIndex(y.months, 12));
  json.num("busiestWeekday", stats::busiestIndex(weekdays, 7));
  json.num("favouriteHour", stats::busiestIndex(hours, 24));
  json.key("months");
  out.nums(y.months, 12);
  json.key("weekdays");
  out.nums(weekdays, 7);
  json.key("hours");
  out.nums(hours, 24);
  json.key("styles");
  {
    JsonObjectWriter styles(out);
    for (int i = 1; i < stats::STYLE_COUNT; ++i)
      styles.num(stats::styleKey(static_cast<stats::ReadingStyle>(i)), y.styles[i]);
    styles.end();
  }
  json.key("topBooks");
  out.text("[");
  for (int i = 0; i < y.topCount; ++i) {
    if (i > 0) out.text(",");
    JsonObjectWriter top(out);
    top.num("book", y.topBooks[i]);
    names->load(y.topBooks[i]);
    top.str("title", names->title());
    top.num("sec", y.topSec[i]);
    top.end();
  }
  out.text("]");
  json.end();
  out.end();
}

void handleSpeed(WebServer& server) {
  READING_STATS.prepareSummaries();
  auto trends = makeUniqueNoThrow<stats::SpeedTrends>();
  if (!trends) {
    sendOutOfMemory(server, "speed", sizeof(stats::SpeedTrends));
    return;
  }
  stats::loadSpeedTrends(*trends);
  const stats::SpeedTrends& t = *trends;

  startJson(server);
  ReplySink out(server);
  JsonObjectWriter json(out);
  json.num("firstMonth", t.firstMonth);
  json.key("months");
  out.text("[");
  for (int i = 0; i < stats::SPEED_MONTHS; ++i) {
    if (i > 0) out.text(",");
    writeSpeedCell(out, t.months[i]);
  }
  out.text("]");
  json.key("dayParts");
  out.text("[");
  for (int i = 0; i < stats::DAY_PART_COUNT; ++i) {
    if (i > 0) out.text(",");
    writeSpeedCell(out, t.dayParts[i]);
  }
  out.text("]");
  json.key("fonts");
  out.text("[");
  for (int i = 0; i < t.fontCount; ++i) {
    if (i > 0) out.text(",");
    JsonObjectWriter font(out);
    font.num("size", t.fontSizes[i]);
    font.num("pagesPerHour", stats::cellPagesPerHour(t.fonts[i]));
    font.num("measuredSec", static_cast<int64_t>(t.fonts[i].learnedMs / 1000));
    font.end();
  }
  out.text("]");
  json.end();
  out.end();
}

void handleSessionsCsv(WebServer& server) {
  READING_STATS.prepareSummaries();
  // The open registry and one book's names (about 200 bytes).
  auto names = makeUniqueNoThrow<stats::BookNames>();
  if (!names) {
    sendOutOfMemory(server, "names", sizeof(stats::BookNames));
    return;
  }
  names->open();
  startCsv(server, "reading-sessions.csv");
  ReplySink out(server);
  stats::writeSessionsCsvHeader(out);
  struct Context {
    ReplySink* out;
    stats::BookNames* names;
    bool ignoreShort;
    uint32_t rows;
    bool stop;
  } context{&out, names.get(), ignoreShort(), 0, false};
  stats::forEachSession(
      [](void* ctx, const stats::SessionRecord& session, const bool live, const bool hidden) {
        auto& x = *static_cast<Context*>(ctx);
        if (live) return;
        x.names->load(session.bookIndex);
        const stats::SessionExport row{
            session, x.names->title(), x.names->author(), hidden, stats::countsInTotals(session, x.ignoreShort), false};
        stats::writeSessionCsv(*x.out, row);
        x.stop = !x.out->alive(++x.rows);
      },
      &context, &context.stop);
  out.end();
}

void handleBooksCsv(WebServer& server) {
  READING_STATS.prepareSummaries();
  startCsv(server, "reading-books.csv");
  ReplySink out(server);
  stats::writeBooksCsvHeader(out);
  struct Context {
    ReplySink* out;
    uint32_t rows;
  } context{&out, 0};
  stats::forEachBook(
      [](void* ctx, const stats::BookExport& book) {
        auto& x = *static_cast<Context*>(ctx);
        stats::writeBookCsv(*x.out, book);
        return x.out->alive(++x.rows);
      },
      &context);
  out.end();
}

}  // namespace statsweb
