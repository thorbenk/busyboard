#include "phone.h"

#include <pico/stdlib.h>

#include <iostream>

#include "color.h"

#define FADE_IN_FRAMES = 60;
#define PHONE_LED_COUNT 9
#define FPS 60
#define APPEAR_FRAMES 10
#define REANIMATE_NUMBERS_AFTER_FRAMES 10 * 60

void Phone::next_frame() { frame_++; }

void Phone::switch_on(bool enabled) {
  enabled_ = enabled;
  frame_ = 0;
}

void Phone::set_number(uint8_t num) { dialed_number_ = num; }

void Phone::reset() {
  frame_ = 0;
  dialed_number_ = -1;
  last_edge_time_ = -1;
  num_ = 0;
}

void Phone::loop(bool dial_in_progress, bool num_switched) {
  auto new_state = state_;

  if (dial_in_progress != last_dial_in_progress_) {
    if (dial_in_progress) {
      std::cout << "DIALING STARTS" << std::endl;
      reset();
      new_state = State::Dialing;
    } else {
      std::cout << "DIALING ENDS" << std::endl;
      new_state = State::NumberDisplay;
      frame_ = 0;
    }
  }

  if (state_ == State::Dialing || new_state == State::Dialing) {
    if (last_num_switch_ != num_switched) {
      if (last_edge_time_ < 0) {
        last_edge_time_ = to_ms_since_boot(get_absolute_time());
      } else if (to_ms_since_boot(get_absolute_time()) - last_edge_time_ >= 5) {
        std::cout << "PHONE NUM " << num_ << std::endl;
        num_ += 1;
      }
    }
  }

  if (state_ != new_state && new_state == State::NumberDisplay) {
    num_ = num_ / 2 + 1;
    if (num_ >= 10 || num_ < 0) {
      num_ = 0;
    }
    dialed_number_ = num_;
    std::cout << "NEW DIALED NUM " << (int)dialed_number_ << std::endl;
  }

  state_ = new_state;
  last_dial_in_progress_ = dial_in_progress;
  last_num_switch_ = num_switched;
}

void Phone::calc_frame(PicoLed::Color *strip_begin) {
  if (!enabled_) {
    std::fill(strip_begin, strip_begin + PHONE_LED_COUNT,
              PicoLed::RGBW(0, 0, 0, 0));
  } else {
    if (state_ == State::Dialing) {
      float angle = 360.f * (frame_ % (2 * FPS)) / float(2 * FPS);
      float step = 360.f / float(PHONE_LED_COUNT);
      for (int i = 0; i < PHONE_LED_COUNT; ++i) {
        auto const [r, g, b] =
            hsv_to_rgb(std::fmod(angle + i * step, 360.f), 1.0, 255);
        *(strip_begin + i) = PicoLed::RGB(r, g, b);
      }
    } else if (state_ == State::NumberDisplay) {
      // Let the green dots appear one by one
      auto n = std::min(frame_ / APPEAR_FRAMES,
                        static_cast<uint32_t>(PHONE_LED_COUNT));
      for (int i = 0; i < n; ++i) {
        *(strip_begin + i) = i < dialed_number_ ? PicoLed::RGB(0, 32, 0)
                                                : PicoLed::RGB(32, 0, 0);
      }
      for (int i = n; i < PHONE_LED_COUNT; ++i) {
        *(strip_begin + i) = PicoLed::RGBW(0, 0, 0, 0);
      }

      if (frame_ > REANIMATE_NUMBERS_AFTER_FRAMES) {
        frame_ = 0;
      }
    } else if (state_ == State::Idle) {
      std::fill(strip_begin, strip_begin + PHONE_LED_COUNT,
                PicoLed::RGBW(0, 0, 0, 32));
    }
  }

  next_frame();
}
