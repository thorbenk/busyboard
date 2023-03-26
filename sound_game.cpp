#include "sound_game.h"

#include "color.h"

#include <algorithm>
#include <iostream>

#define FADE_IN_FRAMES 120
#define FADE_OUT_FRAMES 120
#define CONSTANT_FRAMES 5 * 60
#define COLOR_CHANGE_FRAMES 5 * 60
#define COLOR_CHANGE_IN 20
#define COLOR_CHANGE_OUT 20
#define COLOR_CHANGE_CHANGE 15

SoundGame::SoundGame() {
  for (int i = 0; i < 8; ++i) {
    hues_[i] = i * 360 / 8;
  }
}

void SoundGame::next_frame() {
  frame_++;

  if (state_ == State::FadeIn && frame_ >= FADE_IN_FRAMES) {
    state_ = State::Constant;
    frame_ = 0;
  } else if (state_ == State::Constant && frame_ >= CONSTANT_FRAMES) {
    state_ = State::FadeOut;
    frame_ = 0;
  } else if (state_ == State::FadeOut && frame_ >= FADE_OUT_FRAMES) {
    state_ = State::ColorChange;
    std::random_shuffle(permutation_.begin(), permutation_.end());
    frame_ = 0;
  } else if (state_ == State::ColorChange && frame_ >= COLOR_CHANGE_FRAMES) {
    state_ = State::FadeIn;
    frame_ = 0;
  }
}

void SoundGame::calc_frame(PicoLed::Color *strip_begin) {
  uint8_t v = 0;
  if (state_ == State::FadeIn) {
    v = 128 * static_cast<float>(frame_) / FADE_IN_FRAMES;
  } else if (state_ == State::Constant) {
    v = 128;
  } else if (state_ == State::FadeOut) {
    v = 128 - 128 * static_cast<float>(frame_) / FADE_OUT_FRAMES;
  } else if (state_ == State::ColorChange) {
    if (frame_ < COLOR_CHANGE_IN) {
      v = 0;
    } else if (frame_ > COLOR_CHANGE_FRAMES - COLOR_CHANGE_OUT) {
      v = 0;
    } else {
      v = 128;
    }

    if (((frame_ - COLOR_CHANGE_IN) % COLOR_CHANGE_CHANGE) == 0) {
      std::random_shuffle(permutation_.begin(), permutation_.end());
    }
  }

  for (int i = 0; i < 8; ++i) {
    auto const [r, g, b] = hsv_to_rgb(hues_[permutation_[i]], 1.0, v);
    *(strip_begin + i) = PicoLed::RGB(r, g, b);
    // std::cout << (int)r << " " << (int)g << " " << (int)b << std::endl;
  }
  // std::fill(strip_begin, strip_begin+8, PicoLed::RGB(64,64,64));

  next_frame();
}
