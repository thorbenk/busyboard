#include "sound_game.h"

#include "color.h"

#include <algorithm>
#include <iostream>
#include <numeric>

#define FADE_IN_FRAMES 120
#define FADE_OUT_FRAMES 120
#define CONSTANT_FRAMES 15 * 60
#define COLOR_CHANGE_FRAMES 2 * 60
#define COLOR_CHANGE_IN 20
#define COLOR_CHANGE_OUT 20
#define COLOR_CHANGE_CHANGE 10

SoundGame::SoundGame() {
  for (int i = 0; i < 8; ++i) {
    hues_[i] = i * 360 / 8;
  }
}

uint8_t SoundGame::permutation(uint8_t button) { return permutation_[button]; }

bool SoundGame::should_play_sound() { return state_ == State::Constant; }

void SoundGame::set_enabled(bool enabled) {
  enabled_ = enabled;
  frame_ = 0;
  if (enabled)
    state_ = State::FadeIn;
  else
    state_ = State::FadeOut;
}

void SoundGame::next_frame() {
  frame_++;

  if (state_ == State::FadeIn && frame_ >= FADE_IN_FRAMES) {
    // reset to same permutation for now
    // std::iota(permutation_.begin(), permutation_.end(), 0);
    state_ = State::Constant;
    frame_ = 0;
  } else if (state_ == State::Constant && frame_ >= CONSTANT_FRAMES) {
    state_ = State::FadeOut;
    frame_ = 0;
  } else if (state_ == State::FadeOut && frame_ >= FADE_OUT_FRAMES) {
    // state_ = State::ColorChange;
    // std::random_shuffle(permutation_.begin(), permutation_.end());
    if (enabled_) {
      state_ = State::FadeIn;
      frame_ = 0;
    } else {
      state_ = State::Off;
      frame_ = 0;
    }
  } else if (state_ == State::ColorChange && frame_ >= COLOR_CHANGE_FRAMES) {
    state_ = State::FadeIn;
    frame_ = 0;
  }
}

void SoundGame::calc_frame(PicoLed::Color *strip_begin) {
  uint8_t v = 0;
  if (state_ == State::Off) {
    v = 0;
  } else if (state_ == State::FadeIn) {
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
  }

  next_frame();
}

ArcadeSounds SoundGame::sound_for_button(uint8_t button) {
  uint8_t sound_num = permutation(button);

  ArcadeSounds sound;
  switch (sound_num) {
  case 0: {
    sound = ArcadeSounds::transformation__beam_me_up_1;
    break;
  }
  case 1: {
    sound = ArcadeSounds::transformation__swirl_1;
    break;
  }
  case 2: {
    sound = ArcadeSounds::blips_and_beeps__bing_1;
    break;
  }
  case 3: {
    sound = ArcadeSounds::sweeps__down_1;
    break;
  }
  case 4: {
    sound = ArcadeSounds::sweeps__up_1;
    break;
  }
  case 5: {
    sound = ArcadeSounds::score_sounds__coins_1;
    break;
  }
  case 6: {
    sound = ArcadeSounds::score_sounds__score_4;
    break;
  }
  case 7: {
    sound = ArcadeSounds::noise_and_engine__zapping;
    break;
  }
  }

  return sound;
}
