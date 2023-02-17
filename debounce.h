#pragma once
#include "pico/stdlib.h"

int64_t debounce_alarm(alarm_id_t id, void *user_data);

class Debounce {
public:
  Debounce(uint8_t pin, uint32_t debounce_ms)
      : pin_(pin), debounce_ms_(debounce_ms) {}
  void on_event(uint32_t events) {
    if (!alarm_running_) {
      add_alarm_in_ms(debounce_ms_, debounce_alarm, this, false);
      alarm_running_ = true;
    }
  }

  bool state() const { return state_; }

  bool alarm_running_ = false;
  uint pin_;
  uint32_t debounce_ms_ = 4;
  volatile bool state_ = true;
};
