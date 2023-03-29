#include "arcade_buttons.h"

#include "color.h"
#include "gamma8.h"

#define FPS 60
#define ARCADE_BUTTONS_8_LED_LENGTH 8

void ArcadeButtons::set_enabled(bool enabled) { enabled_ = enabled; }

void ArcadeButtons::set_hues(float hue_on, float hue_off) {
  hue_on_ = hue_on;
  hue_off_ = hue_off;
}

void ArcadeButtons::calc_frame(uint32_t frame, uint8_t buttons_8,
                               PicoLed::Color *strip_begin) {
  auto constexpr duration_sec = 2;
  auto constexpr duration_frames = duration_sec * FPS;
  auto constexpr duration_frames_2 = duration_sec * FPS / 2;

  auto f = frame % duration_frames;
  if (f > duration_frames_2)
    f = duration_frames - f;
  uint8_t v = 96 + 32 * (f / static_cast<float>(duration_frames_2));
  v = gamma8[v];

  for (int i = 0; i < ARCADE_BUTTONS_8_LED_LENGTH; ++i) {
    float const v_arcade = enabled_ ? v : 0.f;
    auto const hue = ((1 << i) & buttons_8) ? hue_on_ : hue_off_;
    auto const [r, g, b] = hsv_to_rgb(hue, 1.0, v_arcade);
    *(strip_begin + i) = PicoLed::RGB(r, g, b);
  }
}
