#include "sound_game.h"

#include "color.h"
#include "gamma8.h"

#include <algorithm>
#include <numeric>

#define FADE_IN_FRAMES 120
#define FADE_OUT_FRAMES 120
#define CONSTANT_FRAMES 15 * 60
#define COLOR_CHANGE_FRAMES 2 * 60
#define COLOR_CHANGE_IN 20
#define COLOR_CHANGE_OUT 20
#define COLOR_CHANGE_CHANGE 10
#define PRESSED_HIGHLIGHT_TIME 60

SoundGame::SoundGame() {
  for (int i = 0; i < 8; ++i) {
    hues_[i] = i * 360 / 8;
  }
}

uint8_t SoundGame::permutation(uint8_t button) { return permutation_[button]; }

bool SoundGame::should_play_sound() { return state_ == State::Constant; }

void SoundGame::set_enabled(bool enabled) {
  enabled_ = enabled;
  state_frame_start_ = 0;
  if (enabled)
    state_ = State::FadeIn;
  else
    state_ = State::FadeOut;
}

void SoundGame::next_frame(uint32_t frame) {
  auto const frames_in_state = frame - state_frame_start_;

  if (state_ == State::FadeIn && frames_in_state >= FADE_IN_FRAMES) {
    // reset to same permutation for now
    // std::iota(permutation_.begin(), permutation_.end(), 0);
    state_ = State::Constant;
    state_frame_start_ = frame;
  } else if (state_ == State::Constant && frames_in_state >= CONSTANT_FRAMES) {
    state_ = State::FadeOut;
    state_frame_start_ = frame;
  } else if (state_ == State::FadeOut && frames_in_state >= FADE_OUT_FRAMES) {
    // state_ = State::ColorChange;
    // std::random_shuffle(permutation_.begin(), permutation_.end());
    if (enabled_) {
      state_ = State::FadeIn;
      state_frame_start_ = frame;
    } else {
      state_ = State::Off;
      state_frame_start_ = frame;
    }
  } else if (state_ == State::ColorChange &&
             frames_in_state >= COLOR_CHANGE_FRAMES) {
    state_ = State::FadeIn;
    state_frame_start_ = frame;
  }
}

void SoundGame::calc_frame(uint32_t frame, PicoLed::Color *strip_begin,
                           uint8_t buttons8) {

  auto const frames_in_state = frame - state_frame_start_;

  for (int i = 0; i < 8; ++i) {
    if (buttons8 & (1 << i)) {
      pressed_button_ = i;
      pressed_button_frame_ = frame;
    }
    uint8_t v = 0;
    if (state_ == State::Off) {
      v = 0;
    } else if (pressed_button_ == i) {
      v = 255;
    } else if (state_ == State::FadeIn) {
      v = gamma8[64 +
                 static_cast<uint8_t>(64 * static_cast<float>(frames_in_state) /
                                      FADE_IN_FRAMES)];
    } else if (state_ == State::Constant) {
      v = gamma8[128];
    } else if (state_ == State::FadeOut) {
      v = gamma8[64 + static_cast<uint8_t>(
                          64 - 64 * static_cast<float>(frames_in_state) /
                                   FADE_OUT_FRAMES)];
    } else if (state_ == State::ColorChange) {
      if (frames_in_state < COLOR_CHANGE_IN) {
        v = 0;
      } else if (frames_in_state > COLOR_CHANGE_FRAMES - COLOR_CHANGE_OUT) {
        v = 0;
      } else {
        v = 128;
      }

      if (((frames_in_state - COLOR_CHANGE_IN) % COLOR_CHANGE_CHANGE) == 0) {
        std::random_shuffle(permutation_.begin(), permutation_.end());
      }
    }

    auto const [r, g, b] = hsv_to_rgb(hues_[permutation_[i]], 1.0, v);
    *(strip_begin + i) = PicoLed::RGB(r, g, b);
  }

  if (pressed_button_ >= 0 &&
      frame > pressed_button_frame_ + PRESSED_HIGHLIGHT_TIME) {
    pressed_button_ = -1;
  }

  next_frame(frame);
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
