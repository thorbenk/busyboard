#pragma once

#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include <cmath>
#include <cstdint>
#include <iostream>

namespace detail {
extern uint8_t microsteps_sine_[24];
extern uint16_t accel_table_[32];
constexpr int microstep_wait_usec_begin_ = 1 * 2400;
constexpr float accel_ = 0.95;
constexpr uint8_t accel_rampup_ = 2;
} // namespace detail

template <uint8_t PIN_1, uint8_t PIN_2_3, uint8_t PIN_4> class Vid28Stepper {
public:
  Vid28Stepper() = default;

  void init() {
    constexpr uint8_t pins[3] = {PIN_1, PIN_2_3, PIN_4};
    for (auto i = 0; i < 3; ++i) {
      auto pin = pins[i];
      gpio_set_function(pin, GPIO_FUNC_PWM);
      pwm_slices_[i] = pwm_gpio_to_slice_num(pin);
      pwm_channels_[i] = pwm_gpio_to_channel(pin);
      pwm_config config = pwm_get_default_config();
      pwm_config_set_wrap(&config, 127);
      pwm_init(pwm_slices_[i], &config, true);
    }

    detail::accel_table_[0] = detail::microstep_wait_usec_begin_;
    for (auto i = 1; i < 32; ++i) {
      detail::accel_table_[i] =
          detail::accel_ * static_cast<float>(detail::accel_table_[i - 1]);
    }

    std::cout << "ACCEL TABLE" << std::endl;
    for (auto i = 0; i < 32; ++i) {
      std::cout << "[" << i << "] = " << detail::accel_table_[i] << std::endl;
    }
  }

  void reset(int direction, float hz) {
    bool forward = (direction > 0) ? true : false;
    step_micros(360 * 12, hz, forward);
    current_angle_ = 0;
  }

  void set_zero(float degrees) { zero_angle_ = std::round(degrees * 12); }

  void step_to(float degrees, float hz) {
    int32_t to_go = std::round(degrees * 12) - (current_angle_ - zero_angle_);
    bool forward = (to_go > 0) ? true : false;
    step_micros(std::abs(to_go), hz, forward);
    current_angle_ += to_go;
    std::cout << "step_to went to angle = " << current_angle_ / 12.0
              << std::endl;
  }

private:
  auto step_micros(uint microstep_count, float hz, bool forward) {
    uint64_t us_per_microstep = 1.0 / (hz * 1.0E-6 * 360 * 12);
    std::cout << "I will wait " << us_per_microstep << " us between steps"
              << std::endl;
    uint16_t wait_us;
    for (auto s = 0; s < microstep_count; ++s) {
      if (s < 2 * detail::accel_rampup_ * 32) {
        wait_us = detail::microstep_wait_usec_begin_;
      } else if (s < detail::accel_rampup_ * 32) {
        wait_us = detail::accel_table_[s / detail::accel_rampup_];
        // std::cout << wait_us << " rampup " << std::endl;
      } else if (s >= microstep_count - detail::accel_rampup_ * 32) {
        wait_us = detail::accel_table_[31 - (microstep_count - s) /
                                                detail::accel_rampup_];
        // std::cout << wait_us << " rampdown " << std::endl;
      } else {
        wait_us = detail::accel_table_[31];
      }
      microstep(forward);
      sleep_us(wait_us);
      sleep_ms(1);
    }
    // stop();
  }

  void stop() {
    pwm_set_gpio_level(PIN_1, 0);
    pwm_set_gpio_level(PIN_2_3, 0);
    pwm_set_gpio_level(PIN_4, 0);
  }

  // One microstep moves 1/12 degree
  void microstep(bool forward) {
    constexpr uint8_t pins[3] = {PIN_1, PIN_2_3, PIN_4};
    for (uint8_t i = 0; i < 3; ++i) {
      uint8_t const level = detail::microsteps_sine_[current_microstep_[i]];

      if (forward)
        current_microstep_[i] = (current_microstep_[i] + 1) % 24;
      else {
        if (current_microstep_[i] == 0)
          current_microstep_[i] = 23;
        else
          --current_microstep_[i];
      }
      pwm_set_gpio_level(pins[i], level);
    }
  }

  // There are 24 microsteps for a full rotation of the motor.
  // Phase difference of the three sine waves is 120 deg or 24/3 = 8.
  int8_t current_microstep_[3] = {0, 8, 16};
  int8_t pwm_slices_[3];
  int8_t pwm_channels_[3];

  // Current angle in 1/12 degrees
  int32_t current_angle_ = 0;
  int32_t zero_angle_ = 0;
};
