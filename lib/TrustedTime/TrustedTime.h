#pragma once

#include <cstdint>

// A monotonic floor under the system clock, for loan-expiry enforcement.
//
// The ESP32-C3 keeps time through deep sleep but loses it on power-off, so a
// cold boot starts near epoch 0 and a date-based check would never fire
// offline. Two things lift the clock off that: a battery-backed RTC, seeded
// into the system clock at boot through adopt() — the RTC keeps running
// across power-off, so it is the only real clock while Wi-Fi is down — and
// SNTP whenever Wi-Fi is up. Between those, this module persists the last
// known-good time in NVS (on-flash, not on the removable SD card) and restores
// it into the system clock at boot, so time only ever moves forward across
// power cycles.
//
// The floor can lag real time while the device sits powered off; it can never
// run behind a moment the device has already seen. Enforcement that needs a
// trustworthy "now" uses trustedNow() and fails closed when it returns 0.
namespace trustedtime {

// Restore the persisted floor into the system clock. Call once at boot,
// before anything reads time().
void init();

// Seed the system clock from a battery-backed RTC epoch. Call at boot, before
// init(), so init()'s floor restore can veto a lagging RTC. Also used at runtime
// for network time (SNTP, the Home Backup server's Date header), under the same
// rules. A clock that is already running is never rewound to an older epoch;
// ignored when the epoch is not plausible() or predates the trusted floor.
void adopt(int64_t epoch);

// True when the epoch falls inside the plausible 2025-2100 window and runs at
// most a year ahead of the persisted floor (a reading that far ahead is faulty,
// and the floor never lowers again).
bool plausible(int64_t epoch);

// Persist the floor when the clock advanced past it. Cheap (one NVS read,
// a write only when it moved). Call at sleep entry and after a time sync.
void note();

// Kick off a non-blocking SNTP sync; the sync callback persists the floor.
// Call whenever a Wi-Fi station connection comes up. No-op while running.
void startSync();

// True once after SNTP set the clock (since the last call), so the caller can
// copy the synced time to the RTC on its own task. The callback runs on lwIP.
bool takeSyncEvent();

// Blocking SNTP sync with a deadline, for callers that need the wall clock
// right now (e.g. progress-sync timestamps). Returns true when synced.
bool syncNow(uint32_t timeoutMs);

// Epoch seconds when the clock is trustworthy (a plausible present-day
// value, restored or synced), else 0. Never earlier than a time already seen
// this boot or restored from the floor, so a backward clock step cannot undo
// an expiry. Callers enforcing a date fail closed on 0.
int64_t trustedNow();

}  // namespace trustedtime
