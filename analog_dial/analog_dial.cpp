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
#define FAN_LEDS_LENGTH 6

#define DFPLAYER_UART 1
#define DFPLAYER_MINI_RX 8 /* GP 8 - TX (UART 1) */
#define DFPLAYER_MINI_TX 9 /* GP 9 - RX (UART 1) */

#define SLIDE_POT_GPIO 26

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

  gpio_set_function(LED_FADER, GPIO_FUNC_PWM);
  uint const led_fade_slice_num = pwm_gpio_to_slice_num(LED_FADER);

  pwm_config config = pwm_get_default_config();
  pwm_config_set_wrap(&config, 4095);
  pwm_init(led_fade_slice_num, &config, true);

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

  DfPlayerPico<DFPLAYER_UART, DFPLAYER_MINI_TX, DFPLAYER_MINI_RX> dfp;
  dfp.reset();
  sleep_ms(2000);
  dfp.specifyVolume(10);
  sleep_ms(200);

  phone_dial_leds.fill(PicoLed::RGB(0, 0, 255));
  phone_dial_leds.show();

  int num = -1;

  int display_num = -1;

  uint32_t pulse_time[2] = {time_us_32(), time_us_32()};
  bool last_num_switch = false;

  auto T = time_us_32();

  while (true) {

    if (time_us_32() > 100 * 1000 + T) {
      T = time_us_32();
      uint16_t const result = adc_read();
      float const f = result / 4096.f;
      printf("slider at %f\n", f);

      float const h = f * 360.f;
      float const s = 1.f;
      float const v = 1.f;

      auto const [r, g, b] = hsv_to_rgb(h, s, v);

      uint8_t R = 255 * r;
      uint8_t G = 255 * g;
      uint8_t B = 255 * b;

      printf("R=%d, G=%d, B=%d\n", R, G, B);

      fan_leds.fill(PicoLed::RGB(R, G, B));
      fan_leds.show();

      pwm_set_gpio_level(LED_FADER, result);
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
