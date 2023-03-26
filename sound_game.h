#pragma once

#include <PicoLed.hpp>
#include <array>

class SoundGame {
public:
  enum class State { FadeIn, Constant, FadeOut, ColorChange };

  SoundGame();

  void calc_frame(PicoLed::Color *strip_begin);

  // button in [0, 8)
  uint8_t permutation(uint8_t button);

  bool should_play_sound();

private:
  void next_frame();

  uint16_t hues_[8];
  State state_ = State::FadeIn;
  uint32_t frame_ = 0;
  std::array<uint8_t, 8> permutation_ = {0, 1, 2, 3, 4, 5, 6, 7};
};
