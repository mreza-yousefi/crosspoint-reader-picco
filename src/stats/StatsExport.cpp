#include "StatsExport.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace stats {

namespace {

constexpr char HEX_DIGITS[] = "0123456789abcdef";

void splitLocal(const uint32_t utc, const int16_t tzOffsetMin, int& year, unsigned& month, unsigned& day,
                int& secondOfDay) {
  const int32_t localDays = localDay(utc, tzOffsetMin);
  const int64_t local = static_cast<int64_t>(utc) + static_cast<int64_t>(tzOffsetMin) * 60;
  secondOfDay = static_cast<int>(local - static_cast<int64_t>(localDays) * 86400);
  civilFromDays(localDays, year, month, day);
}

// Percent with one decimal: "99.5".
void formatPercent(const uint16_t bp, char* out, const size_t size) {
  snprintf(out, size, "%u.%u", static_cast<unsigned>(bp / 100), static_cast<unsigned>(bp % 100 / 10));
}

}  // namespace

void TextSink::text(const char* s) { write(s, strlen(s)); }

void formatLocalTime(const uint32_t utc, const int16_t tzOffsetMin, char* out, const size_t size) {
  if (size == 0) return;
  out[0] = '\0';
  if (utc == 0) return;
  int year;
  unsigned month, day;
  int second;
  splitLocal(utc, tzOffsetMin, year, month, day, second);
  snprintf(out, size, "%04d-%02u-%02u %02d:%02d:%02d", year, month, day, second / 3600, second / 60 % 60, second % 60);
}

void formatLocalDate(const uint32_t utc, const int16_t tzOffsetMin, char* out, const size_t size) {
  if (size == 0) return;
  out[0] = '\0';
  if (utc == 0) return;
  int year;
  unsigned month, day;
  int second;
  splitLocal(utc, tzOffsetMin, year, month, day, second);
  snprintf(out, size, "%04d-%02u-%02u", year, month, day);
}

const char* styleKey(const ReadingStyle style) {
  switch (style) {
    case ReadingStyle::Stalled:
      return "stalled";
    case ReadingStyle::Binge:
      return "binge";
    case ReadingStyle::Snacking:
      return "snacking";
    case ReadingStyle::Steady:
      return "steady";
    case ReadingStyle::Intermittent:
      return "intermittent";
    case ReadingStyle::None:
      break;
  }
  return "";
}

const char* endReasonKey(const uint8_t reason) {
  switch (reason) {
    case END_EXIT:
      return "closed";
    case END_SLEEP:
      return "sleep";
    case END_AUTO_SLEEP:
      return "auto_sleep";
    case END_CRASH:
      return "reset";
    default:
      return "";
  }
}

void writeJsonString(TextSink& out, const char* text) {
  out.write("\"", 1);
  const char* run = text;  // bytes not yet written that need no escape
  for (const char* p = text; *p; ++p) {
    const auto c = static_cast<uint8_t>(*p);
    if (c >= 0x20 && c != '"' && c != '\\') continue;
    out.write(run, static_cast<size_t>(p - run));
    run = p + 1;
    if (c == '"' || c == '\\') {
      const char escaped[2] = {'\\', static_cast<char>(c)};
      out.write(escaped, 2);
    } else {
      const char escaped[6] = {'\\', 'u', '0', '0', HEX_DIGITS[c >> 4], HEX_DIGITS[c & 0xF]};
      out.write(escaped, 6);
    }
  }
  out.write(run, strlen(run));
  out.write("\"", 1);
}

void writeCsvField(TextSink& out, const char* text, const bool guardFormula) {
  const bool guard = guardFormula && text[0] != '\0' && strchr("=+-@\t\r", text[0]) != nullptr;
  if (strpbrk(text, ",\"\r\n") == nullptr) {
    if (guard) out.write("'", 1);
    out.text(text);
    return;
  }
  out.write("\"", 1);
  if (guard) out.write("'", 1);
  const char* run = text;
  for (const char* quote = strchr(run, '"'); quote; quote = strchr(run, '"')) {
    out.write(run, static_cast<size_t>(quote - run) + 1);
    out.write("\"", 1);  // a quote is doubled
    run = quote + 1;
  }
  out.text(run);
  out.write("\"", 1);
}

JsonObjectWriter::JsonObjectWriter(TextSink& out) : out(out) { out.write("{", 1); }

void JsonObjectWriter::key(const char* name) {
  if (!first) out.write(",", 1);
  first = false;
  out.write("\"", 1);
  out.text(name);
  out.write("\":", 2);
}

void JsonObjectWriter::num(const char* name, const int64_t value) {
  key(name);
  char buf[24];
  snprintf(buf, sizeof(buf), "%" PRId64, value);
  out.text(buf);
}

void JsonObjectWriter::str(const char* name, const char* value) {
  key(name);
  writeJsonString(out, value);
}

void JsonObjectWriter::flag(const char* name, const bool value) {
  key(name);
  out.text(value ? "true" : "false");
}

void JsonObjectWriter::end() { out.write("}", 1); }

void CsvRowWriter::separator() {
  if (!first) out.write(",", 1);
  first = false;
}

void CsvRowWriter::field(const char* text, const bool guardFormula) {
  separator();
  writeCsvField(out, text, guardFormula);
}

void CsvRowWriter::num(const int64_t value) {
  separator();
  char buf[24];
  snprintf(buf, sizeof(buf), "%" PRId64, value);
  out.text(buf);
}

void CsvRowWriter::end() { out.write("\r\n", 2); }

void writeSessionsCsvHeader(TextSink& out) {
  out.text(CSV_BOM);
  out.text(
      "session,book,title,author,hidden,start_local,end_local,tz_min,duration_sec,active_sec,dict_sec,pages,turns_"
      "forward,"
      "turns_back,jumps,lookups,clippings,bookmarks,start_percent,end_percent,chars_read,learned_ms,font_size,"
      "end_reason,flags,counted,finished\r\n");
}

void writeSessionCsv(TextSink& out, const SessionExport& row) {
  const SessionRecord& s = row.session;
  const uint32_t startUtc = hasWallTime(s) ? s.startUtc : 0;
  char start[24], end[24], percent[8];
  formatLocalTime(startUtc, s.tzOffsetMin, start, sizeof(start));
  formatLocalTime(startUtc == 0 ? 0 : startUtc + s.durationSec, s.tzOffsetMin, end, sizeof(end));

  CsvRowWriter csv(out);
  csv.num(s.sessionSeq);
  csv.num(s.bookIndex);
  csv.field(row.title, true);
  csv.field(row.author, true);
  csv.num(row.hidden ? 1 : 0);
  csv.field(start);
  csv.field(end);
  if (startUtc != 0) {
    csv.num(s.tzOffsetMin);
  } else {
    csv.field("");
  }
  csv.num(s.durationSec);
  csv.num(s.activeSec);
  csv.num(s.dictSec);
  csv.num(paperPagesForward(s));
  csv.num(s.turnsFwd);
  csv.num(s.turnsBack);
  csv.num(s.jumps);
  csv.num(s.lookups);
  csv.num(s.clippings);
  csv.num(s.bookmarks);
  formatPercent(s.startPercentBp, percent, sizeof(percent));
  csv.field(percent);
  formatPercent(s.endPercentBp, percent, sizeof(percent));
  csv.field(percent);
  csv.num(s.charsRead);
  csv.num(s.learnedMs);
  csv.num(s.fontSize);
  csv.field(endReasonKey(s.endReason));
  csv.num(s.flags);
  csv.num(row.counted ? 1 : 0);
  csv.num(finishesBook(s) ? 1 : 0);
  csv.end();
}

void writeSessionJson(TextSink& out, const SessionExport& row) {
  const SessionRecord& s = row.session;
  const uint32_t startUtc = hasWallTime(s) ? s.startUtc : 0;
  char start[24];
  formatLocalTime(startUtc, s.tzOffsetMin, start, sizeof(start));

  JsonObjectWriter json(out);
  json.num("seq", s.sessionSeq);
  json.num("book", s.bookIndex);
  json.num("utc", startUtc);
  json.str("start", start);
  json.num("tz", s.tzOffsetMin);
  json.num("dur", s.durationSec);
  json.num("active", s.activeSec);
  json.num("dict", s.dictSec);
  json.num("pages", paperPagesForward(s));
  json.num("fwd", s.turnsFwd);
  json.num("back", s.turnsBack);
  json.num("jumps", s.jumps);
  json.num("lookups", s.lookups);
  json.num("startBp", s.startPercentBp);
  json.num("endBp", s.endPercentBp);
  json.num("chars", s.charsRead);
  json.num("learnedMs", s.learnedMs);
  json.num("font", s.fontSize);
  json.str("reason", endReasonKey(s.endReason));
  json.num("flags", s.flags);
  json.flag("counted", row.counted);
  json.flag("finished", finishesBook(s));
  json.flag("hidden", row.hidden);
  json.flag("live", row.live);
  json.end();
}

void writeBooksCsvHeader(TextSink& out) {
  out.text(CSV_BOM);
  out.text(
      "book,title,author,hidden,active_sec,sessions,sittings,first_read,last_read,finished,finished_date,"
      "position_percent,furthest_percent,chars_read,learned_ms,pages_per_hour,style\r\n");
}

void writeBookCsv(TextSink& out, const BookExport& row) {
  const BookTotals& t = row.totals;
  char first[12], last[12], finished[12], percent[8];
  formatLocalDate(t.firstUtc, row.firstTz, first, sizeof(first));
  formatLocalDate(t.lastUtc, row.lastTz, last, sizeof(last));
  formatLocalDate(t.finishedUtc > 1 ? t.finishedUtc : 0, row.finishedTz, finished, sizeof(finished));

  CsvRowWriter csv(out);
  csv.num(row.index);
  csv.field(row.title, true);
  csv.field(row.author, true);
  csv.num(row.hidden ? 1 : 0);
  csv.num(t.totalActiveSec);
  csv.num(t.sessions);
  csv.num(t.sittings);
  csv.field(first);
  csv.field(last);
  csv.num(t.finishedUtc != 0 ? 1 : 0);
  csv.field(finished);
  formatPercent(t.lastPercentBp, percent, sizeof(percent));
  csv.field(percent);
  formatPercent(t.maxPercentBp, percent, sizeof(percent));
  csv.field(percent);
  csv.num(t.charsRead);
  csv.num(t.learnedMs);
  csv.num(pagesPerHour(t.charsRead, t.learnedMs));
  csv.field(styleKey(row.style));
  csv.end();
}

void writeBookJson(TextSink& out, const BookExport& row) {
  const BookTotals& t = row.totals;
  char first[12], last[12], finished[12];
  formatLocalDate(t.firstUtc, row.firstTz, first, sizeof(first));
  formatLocalDate(t.lastUtc, row.lastTz, last, sizeof(last));
  formatLocalDate(t.finishedUtc > 1 ? t.finishedUtc : 0, row.finishedTz, finished, sizeof(finished));

  JsonObjectWriter json(out);
  json.num("book", row.index);
  json.str("title", row.title);
  json.str("author", row.author);
  json.flag("hidden", row.hidden);
  json.num("active", t.totalActiveSec);
  json.num("sessions", t.sessions);
  json.num("sittings", t.sittings);
  json.num("lastUtc", t.lastUtc);
  json.str("first", first);
  json.str("last", last);
  json.flag("finished", t.finishedUtc != 0);
  json.str("finishedDate", finished);
  json.num("lastBp", t.lastPercentBp);
  json.num("maxBp", t.maxPercentBp);
  json.num("pagesPerHour", pagesPerHour(t.charsRead, t.learnedMs));
  json.str("style", styleKey(row.style));
  json.end();
}

}  // namespace stats
