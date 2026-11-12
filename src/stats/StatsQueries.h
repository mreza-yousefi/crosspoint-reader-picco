#pragma once

#include <cstdint>
#include <vector>

#include "StatsFormat.h"

// Read side of the reading statistics for the screens. Reads the summaries (days.bin,
// booktotals.bin); the day list streams sessions.bin. Call ReadingStats::prepareSummaries()
// first.
namespace stats {

// Today's local day; known is false when the clock is not trusted (then 0).
int32_t today(bool& known);

struct DayTotals {
  std::vector<DayRecord> days;  // ascending
  uint32_t unknownTimeSec = 0;  // counted reading without wall time
};
// days.bin plus the running session, when it counts.
bool loadDayTotals(DayTotals& out);

// Calls fn for every session that counts in the calendar (countsInCalendar), from
// sessions.bin in file order, then the running session (live = true).
using SessionFn = void (*)(void* ctx, const SessionRecord& session, bool live);
void forEachCalendarSession(SessionFn fn, void* ctx);

struct DaySession {
  SessionRecord record;
  bool live = false;
};
// Calendar sessions that started on a local day.
bool loadDaySessions(int32_t day, std::vector<DaySession>& out);

// Books first finished in a local year (the session that crossed FINISHED_BP), hidden books left out.
int countFinishedInYear(int year);

}  // namespace stats
