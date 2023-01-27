#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include <cmath>
#include <cstdio>
#include <iostream>

#define BUTTON_A_PIN 6
#define BUTTON_B_PIN 7
#define VID28_SHAFT1_PIN1 2
#define VID28_SHAFT1_PIN23 3
#define VID28_SHAFT1_PIN4 4

static const char *gpio_irq_str[] = {
    "LEVEL_LOW",  // 0x1
    "LEVEL_HIGH", // 0x2
    "EDGE_FALL",  // 0x4
    "EDGE_RISE"   // 0x8
};

static char event_str[128];

void gpio_event_string(char *buf, uint32_t events) {
  for (uint i = 0; i < 4; i++) {
    uint mask = (1 << i);
    if (events & mask) {
      // Copy this event string into the user string
      const char *event_str = gpio_irq_str[i];
      while (*event_str != '\0') {
        *buf++ = *event_str++;
      }
      events &= ~mask;

      // If more events add ", "
      if (events) {
        *buf++ = ',';
        *buf++ = ' ';
      }
    }
  }
  *buf++ = '\0';
}

static uint8_t partial_mode[3][6] = {
    {1, 0, 0, 0, 1, 1}, {0, 0, 1, 1, 1, 0}, {1, 1, 1, 0, 0, 0}};

template <uint8_t PIN_1, uint8_t PIN_2_3, uint8_t PIN_4, bool USE_MICROSTEPPING>
class Vid28Stepper {
public:
  Vid28Stepper() = default;

  void init() {
    if constexpr (USE_MICROSTEPPING) {
      static const uint8_t PWM_PINS[] = {PIN_1, PIN_2_3, PIN_4};
      for (auto pin : PWM_PINS) {
        gpio_set_function(pin, GPIO_FUNC_PWM);
        uint const slice = pwm_gpio_to_slice_num(pin);
        pwm_config config = pwm_get_default_config();
        pwm_config_set_wrap(&config, 127);
        pwm_init(slice, &config, true);
      }
    } else {
      gpio_init(PIN_1);
      gpio_set_dir(PIN_1, GPIO_OUT);
      gpio_init(PIN_2_3);
      gpio_set_dir(PIN_2_3, GPIO_OUT);
      gpio_init(PIN_4);
      gpio_set_dir(PIN_4, GPIO_OUT);
    }
  }

  void microstep(bool forward) {
    constexpr uint8_t pins[3] = {PIN_1, PIN_2_3, PIN_4};
    for (uint8_t i = 0; i < 3; ++i) {
      uint8_t const level = microsteps_sine_[current_microstep_[i]];

      //if (i == 0) {
      //  std::cout << "level @ " << (int)current_microstep_[i] << " : " << (int)level << ", [V 3.3] = " << 3.3f * float(level)/127.0 << std::endl;
      //}
      if (forward)
        current_microstep_[i] = (current_microstep_[i] + 1) % 24;
      else
      {
        if (current_microstep_[i] == 0)
          current_microstep_[i] = 23;
        else
          --current_microstep_[i];
      }
      pwm_set_gpio_level(pins[i], level);
    }
  }

  void step_up() {
    for (auto i = 0; i < 6; ++i) {
      auto a = partial_mode[0][i];
      auto b = partial_mode[1][i];
      auto c = partial_mode[2][i];
      std::cout << "step " << i << ": " << (int)a << " " << (int)b << " "
                << (int)c << std::endl;

      gpio_put(PIN_1, partial_mode[0][i]);
      gpio_put(PIN_2_3, partial_mode[1][i]);
      gpio_put(PIN_4, partial_mode[2][i]);
      sleep_ms(33);
    }
    gpio_put(PIN_1, 0);
    gpio_put(PIN_2_3, 0);
    gpio_put(PIN_4, 0);
  }

  void step_down() {
    for (auto i = 0; i < 6; ++i) {
      std::cout << "step " << i << std::endl;
      gpio_put(PIN_1, partial_mode[0][5 - i]);
      gpio_put(PIN_2_3, partial_mode[1][5 - i]);
      gpio_put(PIN_4, partial_mode[2][5 - i]);
      sleep_ms(33);
    }
    gpio_put(PIN_1, 0);
    gpio_put(PIN_2_3, 0);
    gpio_put(PIN_4, 0);
  }

  void test() {
    gpio_put(PIN_1, 1);
    gpio_put(PIN_2_3, 1);
    gpio_put(PIN_4, 1);
  }

private:
  // There are 24 microsteps for a full rotation of the motor.
  // Phase difference of the three sine waves is 120 deg or 24/3 = 8.
  int8_t current_microstep_[3] = {0, 8, 16};
  static uint8_t microsteps_sine_[24];
};

template <uint8_t PIN_1, uint8_t PIN_2_3, uint8_t PIN_4, bool USE_MICROSTEPPING>
uint8_t Vid28Stepper<PIN_1, PIN_2_3, PIN_4,
                     USE_MICROSTEPPING>::microsteps_sine_[24] = {
    114, 102, 88, 72, 55, 39, 25,  13,  5,   1,   1,   5,
    13,  25,  39, 55, 72, 88, 102, 114, 122, 126, 126, 122};

class Debounce {
public:
  Debounce() = default;
  bool edge_is_significant() {
    if (time_ == 0) {
      time_ = time_us_32();
      return true;
    } else if (time_us_32() > time_ + 200 * 1000) {
      time_ = time_us_32();
      return true;
    } else {
      time_ = time_us_32();
      return false;
    }
  }

private:
  uint32_t time_ = 0;
};

Debounce button_a_debounce;
Debounce button_b_debounce;
Vid28Stepper<VID28_SHAFT1_PIN1, VID28_SHAFT1_PIN23, VID28_SHAFT1_PIN4, true>
    stepper;
volatile uint8_t should_step = 0;

void gpio_callback(uint gpio, uint32_t events) {
  auto m = to_ms_since_boot(get_absolute_time());
  if (gpio == BUTTON_A_PIN && button_a_debounce.edge_is_significant()) {
    gpio_event_string(event_str, events);
    printf("[%d] BUTTON A: GPIO %d: %s\n", m, gpio, event_str);
    should_step = 1;
  } else if (gpio == BUTTON_B_PIN && button_b_debounce.edge_is_significant()) {
    gpio_event_string(event_str, events);
    printf("[%d] BUTTON B: GPIO %d: %s\n", m, gpio, event_str);
    should_step = 2;
  }
}

int main() {
  stdio_init_all();

  gpio_init(BUTTON_A_PIN);
  gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
  gpio_pull_up(BUTTON_A_PIN);
  gpio_set_irq_enabled_with_callback(BUTTON_A_PIN, GPIO_IRQ_EDGE_FALL, true,
                                     &gpio_callback);

  gpio_init(BUTTON_B_PIN);
  gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
  gpio_pull_up(BUTTON_B_PIN);
  gpio_set_irq_enabled_with_callback(BUTTON_B_PIN, GPIO_IRQ_EDGE_FALL, true,
                                     &gpio_callback);

  stepper.init();
  while (true) {
    if (should_step > 0) {
      if (should_step == 1) {
        std::cout << "step up" << std::endl;
        // stepper.step_up();
        for (int i = 0; i < 50*24; ++i) {
          stepper.microstep(true);
          sleep_ms(1);
          //sleep_us(500);
        }
        std::cout << "step up done" << std::endl;
      } else if (should_step == 2) {
        std::cout << "step down" << std::endl;
        // stepper.step_down();
        for (int i = 0; i < 50*24; ++i) {
          stepper.microstep(false);
          sleep_ms(1);
          //sleep_us(500);
        }
        std::cout << "step down done" << std::endl;
      }
      // stepper.test();
      should_step = 0;
    }
  }

  return 0;
}
