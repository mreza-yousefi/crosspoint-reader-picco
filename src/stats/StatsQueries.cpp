#include "StatsQueries.h"

#include <HalStorage.h>
#include <TrustedTime.h>

#include "CrossPointSettings.h"
#include "ReadingStats.h"
#include "StatsLogic.h"

namespace stats {

namespace {
bool readExact(HalFile& file, void* out, const size_t size) { return file.read(out, size) == static_cast<int>(size); }

bool ignoreShort() { return SETTINGS.statsIgnoreShortOpens != 0; }

// The running session, when it counts in the calendar.
bool liveCalendarSession(SessionRecord& live) {
  bool hidden = false;
  return READING_STATS.liveSession(live, hidden) && countsInCalendar(live, ignoreShort(), hidden);
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

void forEachCalendarSession(const SessionFn fn, void* ctx) {
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
        if (countsInCalendar(record, ignoreShort(), hidden[record.bookIndex] != 0)) fn(ctx, record, false);
      }
    }
  }
  SessionRecord live{};
  if (liveCalendarSession(live)) fn(ctx, live, true);
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

}  // namespace stats
