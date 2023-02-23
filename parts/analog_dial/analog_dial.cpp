#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
extern "C" {
#include <PicoTM1637.h>
}
#include "dfPlayerDriver.h"
#include <PicoLed.hpp>
#include <cmath>
#include <cstdio>
#include <iostream>

#define LED_FADER 18
#define LED_2 19
#define BUTTON_DIALING_IN_PROGRESS 17
#define BUTTON_NUMBER_BEEPER 16
#define DIGITS_CLK_PIN 21
#define DIGITS_DIO_PIN 20

#define PHONE_LEDS_DATA_PIN 15
#define PHONE_LEDS_LENGTH 10

#define FAN_LEDS_DATA_PIN 14
#define FAN_LEDS_PWM_PIN 13
#define FAN_LEDS_LENGTH 6

#define TEST_LED_DATA_PIN 10

#define DFPLAYER_UART 1
#define DFPLAYER_MINI_RX 8 /* GP 8 - TX (UART 1) */
#define DFPLAYER_MINI_TX 9 /* GP 9 - RX (UART 1) */

#define SLIDE_POT_GPIO 26

#define BUTTON_A_PIN 12
#define BUTTON_B_PIN 11

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

template <uint8_t PIN_1, uint8_t PIN_2_3, uint8_t PIN_4> class Vid28Stepper {
public:
  Vid28Stepper() = default;
  void init() {
    gpio_init(PIN_1);
    gpio_set_dir(PIN_1, GPIO_OUT);
    gpio_init(PIN_2_3);
    gpio_set_dir(PIN_2_3, GPIO_OUT);
    gpio_init(PIN_4);
    gpio_set_dir(PIN_4, GPIO_OUT);
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
      sleep_ms(2000);
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
      sleep_ms(2000);
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
};

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
Vid28Stepper<5, 4, 3> stepper;
uint8_t should_step = 0;

// See
auto hsv_to_rgb(float H, float S, float V) -> std::tuple<float, float, float> {
  float h = std::floor(H / 60.f);
  float f = H / 60.f - h;
  float p = V * (1 - S);
  float q = V * (1 - S * f);
  float t = V * (1 - S * (1 - f));

  if (h == 0 || h == 6)
    return std::make_tuple(V, t, p);
  else if (h == 1)
    return std::make_tuple(q, V, p);
  else if (h == 2)
    return std::make_tuple(p, V, t);
  else if (h == 3)
    return std::make_tuple(p, q, V);
  else if (h == 4)
    return std::make_tuple(t, p, V);
  else if (h == 5)
    return std::make_tuple(V, p, q);
  return std::make_tuple(0, 0, 0);
}

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
  adc_init();
  adc_gpio_init(SLIDE_POT_GPIO);
  adc_select_input(SLIDE_POT_GPIO - 26);

  gpio_init(LED_2);
  gpio_set_dir(LED_2, GPIO_OUT);

  gpio_init(BUTTON_DIALING_IN_PROGRESS);
  gpio_set_dir(BUTTON_DIALING_IN_PROGRESS, GPIO_IN);
  gpio_pull_up(BUTTON_DIALING_IN_PROGRESS);

  gpio_init(BUTTON_NUMBER_BEEPER);
  gpio_set_dir(BUTTON_NUMBER_BEEPER, GPIO_IN);
  gpio_pull_up(BUTTON_NUMBER_BEEPER);

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

  gpio_set_function(LED_FADER, GPIO_FUNC_PWM);
  uint const led_fade_slice_num = pwm_gpio_to_slice_num(LED_FADER);
  pwm_config led_fade_pwm_config = pwm_get_default_config();
  pwm_config_set_wrap(&led_fade_pwm_config, 4095);
  pwm_init(led_fade_slice_num, &led_fade_pwm_config, true);

  gpio_set_function(FAN_LEDS_PWM_PIN, GPIO_FUNC_PWM);
  uint const fan_pwm_slice_num = pwm_gpio_to_slice_num(FAN_LEDS_PWM_PIN);
  pwm_config fan_pwm_config = pwm_get_default_config();
  pwm_set_clkdiv(fan_pwm_slice_num, 20.f);
  pwm_config_set_wrap(&fan_pwm_config, 255);
  pwm_init(fan_pwm_slice_num, &fan_pwm_config, true);

  int const led_pins = (1 << LED_2);

  TM1637_init(DIGITS_CLK_PIN, DIGITS_DIO_PIN);
  TM1637_clear();
  TM1637_set_brightness(3);

  auto phone_dial_leds = PicoLed::addLeds<PicoLed::WS2812B>(
      pio1, 0, PHONE_LEDS_DATA_PIN, PHONE_LEDS_LENGTH, PicoLed::FORMAT_WGRB);
  phone_dial_leds.setBrightness(16);
  phone_dial_leds.clear();
  phone_dial_leds.fill(PicoLed::RGB(255, 0, 0));
  phone_dial_leds.show();

  auto fan_leds = PicoLed::addLeds<PicoLed::WS2812B>(
      pio1, 1, FAN_LEDS_DATA_PIN, FAN_LEDS_LENGTH, PicoLed::FORMAT_GRB);
  fan_leds.setBrightness(255);
  fan_leds.clear();
  fan_leds.fill(PicoLed::RGB(0, 255, 0));
  fan_leds.show();

  auto test_led = PicoLed::addLeds<PicoLed::WS2812B>(pio1, 2, TEST_LED_DATA_PIN,
                                                     1, PicoLed::FORMAT_RGB);
  test_led.setBrightness(255);
  test_led.clear();
  test_led.fill(PicoLed::RGB(0, 0, 255));
  test_led.show();

  DfPlayerPico<DFPLAYER_UART, DFPLAYER_MINI_TX, DFPLAYER_MINI_RX> dfp;
  dfp.reset();
  sleep_ms(2000);
  dfp.specifyVolume(10);
  sleep_ms(200);

  phone_dial_leds.fill(PicoLed::RGB(0, 0, 255));
  phone_dial_leds.show();

  stepper.init();

  int num = -1;

  int display_num = -1;

  uint32_t pulse_time[2] = {time_us_32(), time_us_32()};
  bool last_num_switch = false;

  auto T = time_us_32();

  std::cout << "button A" << gpio_get(BUTTON_A_PIN) << std::endl;
  std::cout << "button B" << gpio_get(BUTTON_B_PIN) << std::endl;

  while (true) {
    if (should_step > 0) {
      if (should_step == 1) {
        std::cout << "step up" << std::endl;
        stepper.step_up();
      } else if (should_step == 2) {
        std::cout << "step down" << std::endl;
        stepper.step_down();
      }
      // stepper.test();
      should_step = 0;
    }

    if (time_us_32() > 100 * 1000 + T) {
      T = time_us_32();
      uint16_t const result = adc_read();
      float const f = result / 4096.f;

      float const h = f * 360.f;
      float const s = 1.f;
      float const v = 1.f;

      auto const [r, g, b] = hsv_to_rgb(h, s, v);

      uint8_t R = 255 * r;
      uint8_t G = 255 * g;
      uint8_t B = 255 * b;

      fan_leds.fill(PicoLed::RGB(R, G, B));
      fan_leds.show();

      pwm_set_gpio_level(LED_FADER, result);

      // Lowest speed should be 10%
      // The PWM is inverted:
      uint8_t fan_pwm = static_cast<uint8_t>(std::max(25.5f, f * 255.f));

      // printf("slider at %f\n", f);
      // printf("R=%d, G=%d, B=%d\n", R, G, B);
      // printf("fan pwm = %d\n", fan_pwm);

      pwm_set_gpio_level(FAN_LEDS_PWM_PIN, fan_pwm);

      test_led.fill(PicoLed::RGB(R, G, B));
      test_led.show();
    }

    int mask = 0;

    bool const dial_in_progress = !gpio_get(BUTTON_DIALING_IN_PROGRESS);
    bool const num_switched = gpio_get(BUTTON_NUMBER_BEEPER);

    mask |= num_switched << LED_2;

    gpio_put_masked(led_pins, mask);

    pulse_time[num_switched] = time_us_32();

    if (dial_in_progress) {
      if (num == -1) {
        TM1637_clear();
        num = 0;
      };

      uint32_t t1 = pulse_time[0];
      uint32_t t2 = pulse_time[1];
      if (t1 > t2) {
        uint32_t tmp = t1;
        t1 = t2;
        t2 = tmp;
      }

      if (last_num_switch != num_switched && t2 - t1 > 5 * 1000) {
        num += 1;
        last_num_switch = num_switched;
      }
    } else {
      if (display_num != num && num > 0) {
        num = num / 2;
        if (num == 10) {
          num = 0;
        }
        TM1637_display(num, false);

        uint8_t folder = 1;
        uint8_t track = static_cast<uint8_t>(num);

        uint16_t cmd = (folder << 8) | num;

        dfp.sendCmd(dfPlayer::SPECIFY_FOLDER_PLAYBACK, cmd);
        printf("%04X\n", cmd);
        sleep_ms(200);
        printf("display %i\n", num);
        for (int i = 0; i < num; ++i) {
          phone_dial_leds.setPixelColor(i, PicoLed::RGB(0, 255, 0));
        }
        for (int i = num; i < 10; ++i) {
          phone_dial_leds.setPixelColor(i, PicoLed::RGB(255, 0, 0));
        }
        phone_dial_leds.show();
        display_num = num;
        num = -1;
        last_num_switch = false;
      }
    }
  }

  return 0;
}
