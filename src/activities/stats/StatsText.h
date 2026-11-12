#pragma once

#include <cstddef>
#include <cstdint>

// Text for the reading-stats screens: durations, dates and clock times in the UI language.
namespace statstext {

// "1 h 05 min", "12 min".
void duration(uint32_t seconds, char* out, size_t size);
// Axis and bar labels: "45m", "2h", "1.5h".
void compactDuration(uint32_t seconds, char* out, size_t size);
// Short month (1-12) and weekday (Monday = 0) names.
void monthName(unsigned month, char* out, size_t size);
void weekdayName(int weekday, char* out, size_t size);
// "Mon 3 Nov 2026"; the year only when withYear.
void dayLong(int32_t day, bool withYear, char* out, size_t size);
// "3 Nov".
void dayShort(int32_t day, char* out, size_t size);
// Minute of the day as "21:05" or "9:05 PM", following the clock setting.
void clockTime(int minuteOfDay, char* out, size_t size);

}  // namespace statstext
