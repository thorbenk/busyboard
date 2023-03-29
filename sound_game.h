#pragma once

#include <PicoLed.hpp>
#include <array>

#include "arcade_sounds.h"

class SoundGame {
public:
  enum class State { Off, FadeIn, Constant, FadeOut, ColorChange };

  SoundGame();

  void calc_frame(uint32_t frame, PicoLed::Color *strip_begin,
                  uint8_t buttons8);

  void set_enabled(bool);

  // button in [0, 8)
  uint8_t permutation(uint8_t button);

  bool should_play_sound();

  ArcadeSounds sound_for_button(uint8_t button);

private:
  void next_frame(uint32_t frame);

  bool enabled_ = false;
  uint16_t hues_[8];
  State state_ = State::Off;
  uint32_t state_frame_start_ = 0;
  std::array<uint8_t, 8> permutation_ = {0, 1, 2, 3, 4, 5, 6, 7};
  int8_t pressed_button_ = -1;
  uint32_t pressed_button_frame_ = 0;
};
