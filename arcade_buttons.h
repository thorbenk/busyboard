#pragma once

#include <PicoLed.hpp>
#include <array>

class ArcadeButtons {
public:
  ArcadeButtons() = default;

  void calc_frame(uint32_t frame, uint8_t buttons_8,
                  PicoLed::Color *strip_begin);

  void set_enabled(bool);
  void set_hues(float hue_on, float hue_off);

private:
  bool enabled_ = false;
  float hue_on_ = 120.f;
  float hue_off_ = 0.f;
};
