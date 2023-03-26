#include "fan_leds.h"

#include <pico/stdlib.h>

#include <iostream>

#include "color.h"

#define FPS 60

void FanLEDs::next_frame() { frame_++; }

void FanLEDs::set_enabled(bool enabled) {
  enabled_ = enabled;
  frame_ = 0;
}

void FanLEDs::set_mode(FaderMode mode) {
  mode_ = mode;
  frame_ = 0;
}

void FanLEDs::calc_frame(PicoLed::Color *strip_begin, uint8_t led_count,
                         uint8_t *faders) {
  for (int i = 0; i < led_count; ++i) {
    if (enabled_) {
      if (mode_ == FaderMode::RGB) {
        *(strip_begin + i) = PicoLed::RGB(faders[1], faders[2], faders[3]);
      } else if (mode_ == FaderMode::HSV) {
        float h = 360.f * faders[1] / 255.0;
        float s = faders[2] / 255.0;
        float v = faders[3];
        auto const [r, g, b] = hsv_to_rgb(h, s, v);
        *(strip_begin + i) = PicoLed::RGB(r, g, b);
      } else {
        float angle = 360.f * (frame_ % (2 * FPS)) / float(2 * FPS);
        float step = 360.f / float(led_count);
        auto const [r, g, b] =
            hsv_to_rgb(std::fmod(angle + i * step, 360.f), 1.0, 255);
        *(strip_begin + i) = PicoLed::RGB(r, g, b);
      }
    } else {
      *(strip_begin + i) = PicoLed::RGB(0, 0, 0);
    }
  }

  next_frame();
}
