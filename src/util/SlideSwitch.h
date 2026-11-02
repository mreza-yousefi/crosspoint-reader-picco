#pragma once

#include <HalGPIO.h>

#include "CrossPointSettings.h"

// A slide switch owns the state of the function it is bound to, so the UI must
// not offer its own toggle for that function or change it behind the switch.
// Touch controls are the exception: there the switch and the light panel's
// touch tile both toggle the touch lock (see slideSwitchTouchLockEngaged).
inline bool slideSwitchOwns(const uint8_t action) {
  return gpio.hasToggleSwitch() && SETTINGS.slideSwitchAction == action;
}

inline bool slideSwitchOwnsLight() { return slideSwitchOwns(CrossPointSettings::SLIDE_SWITCH_FRONTLIGHT); }

// Set while the light panel is open: it pauses the touch lock so the panel
// (opened with Up+Down under the lock) can be used; closing it re-locks.
inline bool lightPanelPausesTouchLock = false;
// Set while an Up hold dictionary lookup runs under the lock (word
// selection and definitions); cleared when the reader is back in front.
inline bool lookupPausesTouchLock = false;
// Touch-lock override: holding Up on Home unlocks touch for a while. Ends when
// a book opens, on a Down hold, on a switch move (main.cpp), at sleep, or after
// 2 minutes without use (ActivityManager).
inline bool touchLockOverride = false;

// The touch lock is on, even while the light panel pauses it (the chord
// shortcuts stay available in the panel). Bound to touch controls, the switch
// toggles the lock on each move and the panel's touch tile toggles it too, so
// the saved Touch Reader Controls setting (off = locked) is the state, not the
// switch position.
inline bool slideSwitchTouchLockEngaged() {
  return slideSwitchOwns(CrossPointSettings::SLIDE_SWITCH_TOUCH_CONTROLS) &&
         SETTINGS.touchReaderControls == CrossPointSettings::TOUCH_READER_OFF;
}

// The touch lock (see slideSwitchTouchLockEngaged) locks the whole
// touchscreen, gestures included; the physical buttons keep working.
// Flips and saves the touch lock (switch move or the panel's touch tile).
inline void toggleTouchLock() {
  SETTINGS.touchReaderControls = SETTINGS.touchReaderControls == CrossPointSettings::TOUCH_READER_OFF
                                     ? CrossPointSettings::TOUCH_READER_ON
                                     : CrossPointSettings::TOUCH_READER_OFF;
  SETTINGS.saveToFile();
}

inline bool slideSwitchLocksTouch() {
  return slideSwitchTouchLockEngaged() && !lightPanelPausesTouchLock && !lookupPausesTouchLock && !touchLockOverride;
}
