#pragma once

// A definition closed (or left for another word) sooner than this after it
// appeared was a mistaken lookup: it is not saved to the vocabulary history
// and not counted in the reading stats.
constexpr unsigned long LOOKUP_MIN_VISIBLE_MS = 1000;

// shownAtMs: millis() when the definition first reached the screen, 0 = not yet.
constexpr bool lookupShownLongEnough(const unsigned long shownAtMs, const unsigned long nowMs) {
  return shownAtMs != 0 && nowMs - shownAtMs >= LOOKUP_MIN_VISIBLE_MS;
}
