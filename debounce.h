#pragma once
#include "hardware/i2c.h"
#include "pico/stdlib.h"

int64_t debounce_alarm(alarm_id_t id, void *user_data);
int64_t debounce_alarm_edge(alarm_id_t id, void *user_data);

enum class StateChange { None, Rising, Falling };

class DebounceEdge {
public:
  DebounceEdge(uint32_t debounce_ms) : debounce_ms_(debounce_ms) {}
  void on_event(uint32_t events) {
    if (debounce_ms_ == 0) {
      if (events & GPIO_IRQ_EDGE_FALL)
        falling_edge_count_ += 1;
      else if (events & GPIO_IRQ_EDGE_RISE)
        rising_edge_count_ += 1;
    } else {
      if (events & GPIO_IRQ_EDGE_FALL)
        state_pending_ = StateChange::Falling;
      else if (events & GPIO_IRQ_EDGE_RISE)
        state_pending_ = StateChange::Rising;
      else
        state_pending_ = StateChange::None;
      if (!alarm_running_) {
        add_alarm_in_ms(debounce_ms_, debounce_alarm_edge, this, true);
        alarm_running_ = true;
      }
    }
  }

  auto reset() -> void {
    rising_edge_count_ = 0;
    falling_edge_count_ = 0;
  }

  bool alarm_running_ = false;
  uint32_t debounce_ms_ = 4;
  volatile int rising_edge_count_ = 0;
  volatile int falling_edge_count_ = 0;
  volatile mutable StateChange state_pending_ = StateChange::None;
};

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

class Debounce_PCF8575 {
public:
  Debounce_PCF8575(i2c_inst_t *i2c, uint8_t addr, uint32_t debounce_ms);

  auto init() -> void;

  void on_pcf8575_interrupt() { needs_update_ = true; }

  auto loop() -> bool;
  auto state() const -> uint16_t { return debounced_state_; };

private:
  inline auto timed_out(uint8_t i, uint64_t now) const -> bool {
    return now > 0 && now - debounce_timers_[i] > debounce_ms_ * 1000;
  }
  inline auto is_stopped(uint8_t i) const -> bool {
    return debounce_timers_[i] == 0;
  }
  inline auto start_timer(uint8_t i, uint64_t now) -> void {
    debounce_timers_[i] = now;
  }
  inline auto stop_timer(uint8_t i) -> void { debounce_timers_[i] = 0; }
  inline auto timed_out_mask(uint64_t now) {
    uint16_t timeout_mask = 0;
    for (uint8_t i = 0; i < 16; ++i) {
      if (timed_out(i, now)) {
        timeout_mask |= (1 << i);
      }
    }
    return timeout_mask;
  }
  inline auto set_debounced(uint8_t i, bool state) {
    if (state)
      debounced_state_ |= (1 << i);
    else
      debounced_state_ &= ~(1 << i);
  }

  i2c_inst_t *i2c_;
  uint8_t i2c_address_;

  volatile bool needs_update_ = true;
  uint32_t const debounce_ms_ = 4;
  uint16_t debounced_state_ = 0;

  // A zero means: No timer running.
  // A positive value gives the start time of the timer in usec since boot.
  uint64_t debounce_timers_[16];
  bool init_ = true;
};
