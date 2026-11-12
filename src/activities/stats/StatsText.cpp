#include "StatsText.h"

#include <I18n.h>

#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "stats/StatsLogic.h"

namespace statstext {

namespace {
// The n-th word of a translated list separated by single spaces (the month and weekday name lists).
void nthWord(const char* list, int n, char* out, const size_t size) {
  out[0] = '\0';
  const char* p = list;
  for (int i = 0; i < n && p; ++i) {
    p = strchr(p, ' ');
    if (p) ++p;
  }
  if (!p) return;
  const char* end = strchr(p, ' ');
  const size_t len = end ? static_cast<size_t>(end - p) : strlen(p);
  snprintf(out, size, "%.*s", static_cast<int>(len), p);
}
}  // namespace

void duration(const uint32_t seconds, char* out, const size_t size) {
  const uint32_t minutes = (seconds + 30) / 60;
  if (minutes < 60) {
    snprintf(out, size, tr(STR_STATS_MINUTES), static_cast<unsigned>(minutes));
  } else {
    snprintf(out, size, tr(STR_STATS_HOURS_MINUTES), static_cast<unsigned>(minutes / 60),
             static_cast<unsigned>(minutes % 60));
  }
}

void compactDuration(const uint32_t seconds, char* out, const size_t size) {
  const uint32_t minutes = (seconds + 30) / 60;
  if (minutes < 60) {
    snprintf(out, size, "%u%s", static_cast<unsigned>(minutes), tr(STR_STATS_UNIT_MIN));
    return;
  }
  const uint32_t tenths = (seconds + 180) / 360;  // hours, one decimal
  if (tenths < 100 && tenths % 10 != 0) {
    snprintf(out, size, "%u.%u%s", static_cast<unsigned>(tenths / 10), static_cast<unsigned>(tenths % 10),
             tr(STR_STATS_UNIT_HOUR));
  } else {
    snprintf(out, size, "%u%s", static_cast<unsigned>((tenths + 5) / 10), tr(STR_STATS_UNIT_HOUR));
  }
}

void monthName(const unsigned month, char* out, const size_t size) {
  nthWord(tr(STR_STATS_MONTH_NAMES), static_cast<int>(month) - 1, out, size);
}

void weekdayName(const int weekday, char* out, const size_t size) {
  nthWord(tr(STR_STATS_WEEKDAY_NAMES), weekday, out, size);
}

void dayLong(const int32_t day, const bool withYear, char* out, const size_t size) {
  int year;
  unsigned month, dayOfMonth;
  stats::civilFromDays(day, year, month, dayOfMonth);
  char weekdayText[16];
  char monthText[16];
  weekdayName(stats::weekday(day), weekdayText, sizeof(weekdayText));
  monthName(month, monthText, sizeof(monthText));
  if (withYear) {
    snprintf(out, size, "%s %u %s %d", weekdayText, dayOfMonth, monthText, year);
  } else {
    snprintf(out, size, "%s %u %s", weekdayText, dayOfMonth, monthText);
  }
}

void dayShort(const int32_t day, char* out, const size_t size) {
  int year;
  unsigned month, dayOfMonth;
  stats::civilFromDays(day, year, month, dayOfMonth);
  char monthText[16];
  monthName(month, monthText, sizeof(monthText));
  snprintf(out, size, "%u %s", dayOfMonth, monthText);
}

void clockTime(const int minuteOfDay, char* out, const size_t size) {
  const int hour = (minuteOfDay / 60) % 24;
  const int minute = minuteOfDay % 60;
  if (SETTINGS.clockFormat == 1) {
    const int hour12 = hour % 12 == 0 ? 12 : hour % 12;
    snprintf(out, size, "%d:%02d %s", hour12, minute, hour >= 12 ? "PM" : "AM");
  } else {
    snprintf(out, size, "%02d:%02d", hour, minute);
  }
}

}  // namespace statstext
