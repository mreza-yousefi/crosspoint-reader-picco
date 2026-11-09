#include "NetworkTime.h"

#include <HalClock.h>
#include <TrustedTime.h>

#include <ctime>

#include "CrossPointSettings.h"
#include "activities/RenderLock.h"

void applyNetworkTime(const int64_t utc) {
  if (!trustedtime::plausible(utc)) return;
  // Only when the system clock is behind: settimeofday() would drop its fraction of a second.
  if (utc > static_cast<int64_t>(time(nullptr))) trustedtime::adopt(utc);
  if (!halClock.isAvailable()) return;
  // Also covers the settings write: SD and display may share the SPI bus.
  RenderLock lock;
  if (halClock.setUtc(static_cast<time_t>(utc)) && !SETTINGS.clockHasBeenSynced) {
    SETTINGS.clockHasBeenSynced = 1;
    SETTINGS.saveToFile();
  }
}
