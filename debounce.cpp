#include "debounce.h"
#include "hardware/i2c.h"

#include <bitset>
#include <iostream>

namespace {
auto read_pcf8575(i2c_inst_t *i2c, uint8_t addr) -> uint16_t {
  int ret;
  uint8_t rxdata[2];
  ret = i2c_read_timeout_us(i2c, addr, rxdata, 2, true, 5 * 1000);
  if (ret != 2) {
    std::cerr << "read pcf8575 (0x" << std::hex << (int)addr << ") : "
              << "unexpected return value " << std::dec << ret << std::endl;
  }
  return *reinterpret_cast<uint16_t *>(rxdata);
}
} // namespace

int64_t debounce_alarm(alarm_id_t id, void *user_data) {
  Debounce *d = reinterpret_cast<Debounce *>(user_data);
  d->alarm_running_ = false;
  d->state_ = gpio_get(d->pin_);
  return 0;
}

int64_t debounce_alarm_edge(alarm_id_t id, void *user_data) {
  DebounceEdge *d = reinterpret_cast<DebounceEdge *>(user_data);
  if (d->state_pending_ == StateChange::Rising)
    d->rising_edge_count_ += 1;
  if (d->state_pending_ == StateChange::Falling)
    d->falling_edge_count_ += 1;
  d->state_pending_ = StateChange::None;
  d->alarm_running_ = false;
  return 0;
}

Debounce_PCF8575::Debounce_PCF8575(i2c_inst_t *i2c, uint8_t addr,
                                   uint32_t debounce_ms)
    : i2c_(i2c), i2c_address_(addr), debounce_ms_(debounce_ms) {
  std::fill(std::begin(debounce_timers_), std::end(debounce_timers_), 0);
}

auto Debounce_PCF8575::init() -> void {
  debounced_state_ = read_pcf8575(i2c_, i2c_address_);
}

auto Debounce_PCF8575::loop() -> bool {
  auto const now = time_us_64();

  // first check if any of the inputs have a running timer
  // that has timed out (has been running longer than the debounce time)
  auto timeout_mask = timed_out_mask(now);
  if (!needs_update_ && timeout_mask == 0)
    return false;

  // Either a previous interrupt indicated that the values of the pins
  // have changed or we have at least one debounce timer that has timed out.
  uint16_t new_state = read_pcf8575(i2c_, i2c_address_);
  // std::cout << "io16: " << std::bitset<16>(new_state) << std::endl;
  needs_update_ = false;

  bool state_changed = false;

  for (uint8_t i = 0; i < 16; ++i) {
    bool const a = debounced_state_ & (1 << i);
    bool const b = new_state & (1 << i);

    if (a != b && is_stopped(i)) {
      // Pin i changed.
      // Start the debounce timer for it.
      start_timer(i, now);
    } else if (timeout_mask & (1 << i)) {
      // The debounce timer for this pin timed out.
      // Stop the timer, and examine the new_state for this pin.
      stop_timer(i);

      if (a != b) {
        state_changed = true;
        set_debounced(i, b);
      }
    }
  }

  if (init_) {
    init_ = false;
    return true;
  }
  return state_changed;
}
