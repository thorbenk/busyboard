#pragma once

#include <PicoLed.hpp>
#include <array>

class Phone {
public:
  enum class State { Idle, NumberDisplay, Dialing };

  Phone() = default;

  void calc_frame(PicoLed::Color *strip_begin);

  void switch_on(bool);
  void set_number(uint8_t num);

  void loop(bool dial_in_progress, bool num_switched);

  int8_t dialed_number() { return dialed_number_; }

private:
  void next_frame();
  void reset();

  bool enabled_ = false;
  State state_ = State::Idle;
  uint32_t frame_ = 0;
  int8_t dialed_number_ = -1;
  int num_ = -1;
  bool last_num_switch_ = false;
  bool last_dial_in_progress_ = false;

  int64_t last_edge_time_ = -1;
};
