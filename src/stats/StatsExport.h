#pragma once

#include <cstddef>
#include <cstdint>

#include "StatsFormat.h"
#include "StatsLogic.h"

// Text of the stats web page's JSON and the CSV export: escaping, local times and the session
// and book rows. No hardware or storage access (host-tested in test/reading_stats); the columns
// are in docs/reading-stats.md.
namespace stats {

// Where the text goes (the web reply).
class TextSink {
 public:
  virtual void write(const char* data, size_t len) = 0;
  void text(const char* s);

 protected:
  ~TextSink() = default;
};

// "2026-11-17 21:05:09" at the offset (minutes east of UTC); "" when utc is 0.
void formatLocalTime(uint32_t utc, int16_t tzOffsetMin, char* out, size_t size);
// "2026-11-17"; "" when utc is 0.
void formatLocalDate(uint32_t utc, int16_t tzOffsetMin, char* out, size_t size);
// "stalled", "binge", ...; "" for None.
const char* styleKey(ReadingStyle style);
// "closed", "sleep", "auto_sleep", "reset"; "" when unknown.
const char* endReasonKey(uint8_t reason);

// A JSON string with its quotes; control characters, '"' and '\' escaped. Other bytes (UTF-8)
// pass through.
void writeJsonString(TextSink& out, const char* text);
// Starts each CSV file, so spreadsheets read it as UTF-8.
constexpr const char* CSV_BOM = "\xEF\xBB\xBF";
// A CSV field (RFC 4180): quoted when it holds a comma, a quote or a line break. guardFormula:
// text starting with = + - @, TAB or CR gets a leading ' so a spreadsheet does not run it.
void writeCsvField(TextSink& out, const char* text, bool guardFormula = false);

// A JSON object written member by member; names are literals and not escaped.
class JsonObjectWriter {
 public:
  explicit JsonObjectWriter(TextSink& out);  // writes "{"
  // Writes the name; the caller writes the value (an array or an object).
  void key(const char* name);
  void num(const char* name, int64_t value);
  void str(const char* name, const char* value);
  void flag(const char* name, bool value);
  void end();

 private:
  TextSink& out;
  bool first = true;
};

// One CSV line; end() writes CRLF.
class CsvRowWriter {
 public:
  explicit CsvRowWriter(TextSink& out) : out(out) {}
  void field(const char* text, bool guardFormula = false);
  void num(int64_t value);
  void end();

 private:
  void separator();
  TextSink& out;
  bool first = true;
};

struct SessionExport {
  const SessionRecord& session;
  const char* title;
  const char* author;
  bool hidden;   // the book is hidden from the calendar
  bool counted;  // countsInTotals
  bool live;     // the running session
};
// The header writers start with CSV_BOM.
void writeSessionsCsvHeader(TextSink& out);
void writeSessionCsv(TextSink& out, const SessionExport& row);
void writeSessionJson(TextSink& out, const SessionExport& row);

struct BookExport {
  uint16_t index;
  const char* title;
  const char* author;
  const BookTotals& totals;  // booktotals.bin; zero when the book has no counted session
  bool hidden;
  ReadingStyle style;
  // Offsets for the local dates of firstUtc, lastUtc and finishedUtc.
  int16_t firstTz;
  int16_t lastTz;
  int16_t finishedTz;
};
void writeBooksCsvHeader(TextSink& out);
void writeBookCsv(TextSink& out, const BookExport& row);
void writeBookJson(TextSink& out, const BookExport& row);

}  // namespace stats
