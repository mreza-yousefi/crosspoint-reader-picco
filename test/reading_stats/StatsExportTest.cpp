#include <gtest/gtest.h>

#include <string>

#include "StatsExport.h"

using namespace stats;

namespace {

class StringSink final : public TextSink {
 public:
  void write(const char* data, size_t len) override { out.append(data, len); }
  std::string out;
};

std::string json(const char* text) {
  StringSink sink;
  writeJsonString(sink, text);
  return sink.out;
}

std::string csv(const char* text) {
  StringSink sink;
  writeCsvField(sink, text);
  return sink.out;
}

// UTC seconds of 2026-11-17 local time at an offset.
uint32_t utcOf(int hour, int minute, int second, int16_t tzOffsetMin) {
  const int64_t local = static_cast<int64_t>(daysFromCivil(2026, 11, 17)) * 86400 + hour * 3600 + minute * 60 + second;
  return static_cast<uint32_t>(local - tzOffsetMin * 60);
}

SessionRecord session() {
  SessionRecord s{};
  s.sessionSeq = 42;
  s.bookIndex = 3;
  s.tzOffsetMin = 60;
  s.startUtc = utcOf(23, 30, 0, 60);
  s.durationSec = 3600;
  s.activeSec = 3000;
  s.dictSec = 120;
  s.startPaperPage = 10;
  s.endPaperPage = 25;
  s.turnsFwd = 40;
  s.turnsBack = 2;
  s.lookups = 3;
  s.startPercentBp = 9850;
  s.endPercentBp = 10000;
  s.charsRead = 27000;
  s.learnedMs = 2400000;
  s.fontSize = 12;
  s.endReason = END_AUTO_SLEEP;
  s.flags = SESSION_WALL_KNOWN | SESSION_FINISHED;
  return s;
}

}  // namespace

TEST(StatsExport, JsonStringEscapes) {
  EXPECT_EQ(json("plain"), "\"plain\"");
  EXPECT_EQ(json(""), "\"\"");
  EXPECT_EQ(json("say \"hi\" \\ bye"), "\"say \\\"hi\\\" \\\\ bye\"");
  EXPECT_EQ(json("a\nb\tc\x01"), "\"a\\u000ab\\u0009c\\u0001\"");
  // UTF-8 passes through.
  EXPECT_EQ(json("Caf\xc3\xa9"), "\"Caf\xc3\xa9\"");
}

TEST(StatsExport, CsvFieldQuoting) {
  EXPECT_EQ(csv("plain"), "plain");
  EXPECT_EQ(csv(""), "");
  EXPECT_EQ(csv("Smith, John"), "\"Smith, John\"");
  EXPECT_EQ(csv("The \"Best\" Book"), "\"The \"\"Best\"\" Book\"");
  EXPECT_EQ(csv("\"\""), "\"\"\"\"\"\"");
  EXPECT_EQ(csv("line\nbreak"), "\"line\nbreak\"");
  EXPECT_EQ(csv("cr\rhere"), "\"cr\rhere\"");
}

TEST(StatsExport, CsvFormulaGuard) {
  const auto guarded = [](const char* text) {
    StringSink sink;
    writeCsvField(sink, text, true);
    return sink.out;
  };
  EXPECT_EQ(guarded("=SUM(A1)"), "'=SUM(A1)");
  EXPECT_EQ(guarded("+1"), "'+1");
  EXPECT_EQ(guarded("-x"), "'-x");
  EXPECT_EQ(guarded("@me"), "'@me");
  EXPECT_EQ(guarded("\tTab"), "'\tTab");
  EXPECT_EQ(guarded("\rCR"), "\"'\rCR\"");
  EXPECT_EQ(guarded("=A, B"), "\"'=A, B\"");
  EXPECT_EQ(guarded("Plain - title"), "Plain - title");
  EXPECT_EQ(guarded(""), "");
  // Without the guard (numbers and dates) nothing is added.
  EXPECT_EQ(csv("-5"), "-5");
}

TEST(StatsExport, LocalTimeAndOffset) {
  char buf[24];
  formatLocalTime(utcOf(23, 30, 5, 60), 60, buf, sizeof(buf));
  EXPECT_STREQ(buf, "2026-11-17 23:30:05");
  // The same instant at UTC is an hour earlier.
  formatLocalTime(utcOf(23, 30, 5, 60), 0, buf, sizeof(buf));
  EXPECT_STREQ(buf, "2026-11-17 22:30:05");
  // A negative offset can move the date back.
  formatLocalTime(utcOf(1, 0, 0, 0), -330, buf, sizeof(buf));
  EXPECT_STREQ(buf, "2026-11-16 19:30:00");
  formatLocalDate(utcOf(0, 10, 0, 840), 840, buf, sizeof(buf));
  EXPECT_STREQ(buf, "2026-11-17");
  formatLocalTime(0, 60, buf, sizeof(buf));
  EXPECT_STREQ(buf, "");
  formatLocalDate(0, 60, buf, sizeof(buf));
  EXPECT_STREQ(buf, "");
}

TEST(StatsExport, SessionCsvRow) {
  const SessionRecord s = session();
  StringSink sink;
  writeSessionCsv(sink, SessionExport{s, "Dune, Part 1", "Frank \"F\" Herbert", true, true, false});
  EXPECT_EQ(sink.out,
            "42,3,\"Dune, Part 1\",\"Frank \"\"F\"\" Herbert\",1,2026-11-17 23:30:00,2026-11-18 00:30:00,60,3600,3000,"
            "120,15,40,2,0,3,0,0,98.5,100.0,27000,2400000,12,auto_sleep,17,1,1\r\n");

  StringSink header;
  writeSessionsCsvHeader(header);
  EXPECT_EQ(header.out.rfind(std::string(CSV_BOM) + "session,book,title,author,", 0), 0u);
  // As many columns as a row.
  const auto columns = [](const std::string& line) {
    size_t n = 1;
    bool quoted = false;
    for (const char c : line) {
      if (c == '"') quoted = !quoted;
      if (c == ',' && !quoted) ++n;
    }
    return n;
  };
  EXPECT_EQ(columns(header.out), columns(sink.out));
}

TEST(StatsExport, UndatedSessionHasEmptyTimes) {
  SessionRecord s = session();
  s.flags = 0;  // wall time not known
  s.startPaperPage = 0;
  StringSink sink;
  writeSessionCsv(sink, SessionExport{s, "Book", "", false, false, false});
  EXPECT_EQ(sink.out.rfind("42,3,Book,,0,,,,3600,3000,120,0,", 0), 0u) << sink.out;
}

TEST(StatsExport, SessionJson) {
  const SessionRecord s = session();
  StringSink sink;
  writeSessionJson(sink, SessionExport{s, "", "", false, true, true});
  EXPECT_EQ(sink.out,
            "{\"seq\":42,\"book\":3,\"utc\":" + std::to_string(s.startUtc) +
                ",\"start\":\"2026-11-17 23:30:00\",\"tz\":60,\"dur\":3600,\"active\":3000,\"dict\":120,\"pages\":15,"
                "\"fwd\":40,\"back\":2,\"jumps\":0,\"lookups\":3,\"startBp\":9850,\"endBp\":10000,\"chars\":27000,"
                "\"learnedMs\":2400000,\"font\":12,\"reason\":\"auto_sleep\",\"flags\":17,\"counted\":true,"
                "\"finished\":true,\"hidden\":false,\"live\":true}");
}

TEST(StatsExport, BookRows) {
  BookTotals t{};
  t.firstUtc = utcOf(20, 0, 0, 60);
  t.lastUtc = utcOf(23, 30, 0, 60);
  t.totalActiveSec = 7200;
  t.sessions = 4;
  t.sittings = 3;
  t.finishedUtc = 1;  // finished at an unknown time
  t.lastPercentBp = 10000;
  t.maxPercentBp = 10000;
  t.charsRead = 54000;
  t.learnedMs = 3600000;
  const BookExport row{7, "A, B", "", t, true, ReadingStyle::Binge, 60, 60, 60};

  StringSink sink;
  writeBookCsv(sink, row);
  StringSink header;
  writeBooksCsvHeader(header);
  EXPECT_EQ(header.out.rfind(std::string(CSV_BOM) + "book,title,author,hidden,", 0), 0u);
  EXPECT_EQ(sink.out, "7,\"A, B\",,1,7200,4,3,2026-11-17,2026-11-17,1,,100.0,100.0,54000,3600000,30,binge\r\n");

  StringSink js;
  writeBookJson(js, row);
  EXPECT_EQ(js.out,
            "{\"book\":7,\"title\":\"A, B\",\"author\":\"\",\"hidden\":true,\"active\":7200,\"sessions\":4,"
            "\"sittings\":3,\"lastUtc\":" +
                std::to_string(t.lastUtc) +
                ",\"first\":\"2026-11-17\",\"last\":\"2026-11-17\",\"finished\":true,\"finishedDate\":\"\","
                "\"lastBp\":10000,\"maxBp\":10000,\"pagesPerHour\":30,\"style\":\"binge\"}");
}

TEST(StatsExport, StreakCounterMatchesComputeStreaks) {
  const int32_t d = daysFromCivil(2026, 11, 1);
  const DayRecord days[] = {{d, 60, 1, 1, 0, 0},     {d + 1, 60, 1, 1, 0, 0}, {d + 2, 0, 0, 0, 0, 0},
                            {d + 4, 60, 1, 1, 0, 0}, {d + 5, 60, 1, 1, 0, 0}, {d + 6, 60, 1, 1, 0, 0}};
  StreakCounter counter;
  for (const DayRecord& day : days) counter.add(day);
  const Streaks a = counter.result(d + 7);
  const Streaks b = computeStreaks(days, 6, d + 7);
  EXPECT_EQ(a.current, 3);
  EXPECT_EQ(a.longest, 3);
  EXPECT_EQ(a.current, b.current);
  EXPECT_EQ(a.longest, b.longest);
  EXPECT_EQ(counter.result(d + 9).current, 0);
}

TEST(StatsExport, OverviewAccumulator) {
  const int32_t today = daysFromCivil(2026, 11, 17);  // a Tuesday
  const DayRecord days[] = {{daysFromCivil(2025, 12, 31), 100, 1, 1, 0, 0},
                            {daysFromCivil(2026, 1, 2), 200, 1, 1, 0, 0},
                            {daysFromCivil(2026, 11, 1), 400, 1, 1, 0, 0},
                            {daysFromCivil(2026, 11, 16), 800, 1, 1, 0, 0},
                            {today, 1600, 1, 1, 0, 0}};
  OverviewAccumulator known(today, true);
  for (const DayRecord& d : days) known.add(d);
  const Overview o = known.result();
  EXPECT_EQ(o.todaySec, 1600u);
  EXPECT_EQ(o.weekSec, 2400u);  // Monday 16 Nov and today
  EXPECT_EQ(o.monthSec, 2800u);
  EXPECT_EQ(o.yearSec, 3000u);
  EXPECT_EQ(o.daysSec, 3100u);
  EXPECT_EQ(o.anchor, today);
  EXPECT_EQ(o.streaks.current, 2);
  EXPECT_EQ(o.streaks.longest, 2);

  // Without a clock: no period sums; the streaks end at the last day with reading.
  OverviewAccumulator unknown(0, false);
  for (const DayRecord& d : days) unknown.add(d);
  const Overview u = unknown.result();
  EXPECT_EQ(u.todaySec, 0u);
  EXPECT_EQ(u.yearSec, 0u);
  EXPECT_EQ(u.daysSec, 3100u);
  EXPECT_EQ(u.anchor, today);
  EXPECT_EQ(u.streaks.current, 2);

  EXPECT_EQ(OverviewAccumulator(0, false).result().anchor, 0);
}
