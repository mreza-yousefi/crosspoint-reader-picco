#pragma once

#include <cstdint>

// Network time (SNTP, the Home Backup server's Date header) to the clocks: the
// system clock moves forward only (trustedtime::adopt), the RTC, which the
// status bar shows, is set either way. Ignored unless trustedtime::plausible().
// Takes the render lock: the render task reads the RTC cache. Call from the
// loop task, never while holding the render lock.
void applyNetworkTime(int64_t utc);
