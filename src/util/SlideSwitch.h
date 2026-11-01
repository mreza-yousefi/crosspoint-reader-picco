#pragma once

#include <HalGPIO.h>

#include "CrossPointSettings.h"

// A slide switch owns the state of the function it is bound to, so the UI must
// not offer its own toggle for that function or change it behind the switch.
inline bool slideSwitchOwns(const uint8_t action) {
  return gpio.hasToggleSwitch() && SETTINGS.slideSwitchAction == action;
}

inline bool slideSwitchOwnsLight() { return slideSwitchOwns(CrossPointSettings::SLIDE_SWITCH_FRONTLIGHT); }

// Set while the light panel is open: it pauses the touch lock so the panel
// (opened with Up+Down under the lock) can be used; closing it re-locks.
inline bool lightPanelPausesTouchLock = false;

// A switch bound to touch controls and set on (red) locks the whole
// touchscreen, gestures included; the physical buttons keep working.
inline bool slideSwitchLocksTouch() {
  return slideSwitchOwns(CrossPointSettings::SLIDE_SWITCH_TOUCH_CONTROLS) && gpio.isToggleSwitchOn() &&
         !lightPanelPausesTouchLock;
}
