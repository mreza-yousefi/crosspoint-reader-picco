#pragma once

#include "CrossPointSettings.h"
#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "util/ButtonNavigator.h"

// Top-anchored control center opened by a top-edge down-swipe, a status-bar tap,
// or a button bound to "Control Center" (iOS Control Center style): a grabber,
// the frontlight brightness/warmth sliders (on boards with a light), and a row
// of quick-setting icons — night mode, ghost-cleanup refresh, reading
// orientation, reader touch controls, and Home. Enabled toggles have filled
// rounded squares; all other controls are outlined. The grabber sits
// along the panel's bottom edge, the edge the sheet is dragged from.
class FrontlightPanelActivity final : public Activity, private UiAppHost {
  ButtonNavigator buttonNavigator;

  uint8_t brightness = 60;
  uint8_t warmth = 50;
  bool lightOn = false;
  // lightOn is seeded from the live hardware state (Frontlight.isOn()), which
  // legitimately diverges from the saved SETTINGS.frontlightOn preference —
  // e.g. after a wake with frontlightRestoreOnWake off, the light stays off
  // live while the saved "was on" preference is deliberately kept (see
  // main.cpp's restoreLightOn). brightness/warmth have no such divergence
  // (always restored unconditionally on boot), so only lightOn needs a
  // touched-by-the-user flag: onExit() must not persist a mirror that never
  // reflected user intent in the first place.
  bool lightOnChanged = false;
  bool draggingSlider = false;
  // A finger held on a - / + button: which one, and when it steps next.
  freeink::ui::ActionId heldStepAction = freeink::ui::NO_ACTION;
  int16_t heldStepValue = 0;
  unsigned long nextHeldStepMs = 0;
  bool stepRepeated = false;  // the hold stepped: its release adds no tap step
  int panelBottom = 0;
  const char* notice = nullptr;

  // Quick settings, left to right: night mode, refresh, orientation, touch, Home.
  static constexpr int kTileCount = 5;

  // SliderRowProps embeds a StyleSet, so keep it off the render stack.
  freeink::ui::SliderRowProps rowProps;

  static void panelScreen(UiScreen& screen, void* user);
  static void onBrightnessEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onWarmthEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onToggleEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onBrightnessStepEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onWarmthStepEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onTileEvent(const freeink::ui::ActionEvent& event, void* user);

  void buildPanelScreen(UiScreen& screen);
  // One slider row: a caption line (name + live percentage) above
  // [-] [draggable 1-bit capsule] [+], plus a lamp on/off button after the +
  // when showToggle is set (the brightness row).
  // namedEnds: show 0 / 100 as Off / Full (warmth: one colour only there).
  void addSliderRow(UiScreen& screen, const char* label, uint8_t value, freeink::ui::ActionId sliderAction,
                    freeink::ui::ActionId stepAction, bool showToggle, bool namedEnds = false);
  int computePanelBottom() const;
  void adjustBrightness(int delta);
  void adjustWarmth(int delta);
  void toggleLight();
  void repeatHeldStep(const freeink::ui::InputSnapshot& snap);
  void runTile(int idx);
  // Copy the panel's live brightness/warmth/lightOn into SETTINGS and save if
  // anything actually changed. onExit() runs it on every way out.
  void persistLightSettings();
  void close();

  // One-shot: a tile that rewrote the whole frame (night mode) re-drives it
  // with the ghost-cleanup waveform on the next render. The "refresh" tile does
  // not use this — it closes the panel and promotes the repaint underneath
  // instead (GfxRenderer::promoteNextRefresh).
  bool cleanRefreshPending = false;
  // Copy of the frame under the panel, put back on exit for screens that do
  // not repaint all of it (the definition sheet). nullptr = not kept.
  uint8_t* coveredFrame = nullptr;

 public:
  explicit FrontlightPanelActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  // One line under the battery band explaining why the panel opened on its own
  // (e.g. the wake light check). Must outlive the panel; nullptr = none.
  void setNotice(const char* text) { notice = text; }
  ~FrontlightPanelActivity() override;
  // Copies the current frame to put back on exit. Caller holds the render lock.
  void keepCoveredFrame();
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool handleHomeGesture() override;
};
