#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include <algorithm>
#include <bitset>
#include <iostream>
#include <stdio.h>

#include <PicoLed.hpp>

#include "color.h"
#include "debounce.h"

// int main() {
//   stdio_init_all();
//   gpio_init(BUTTON_PIN);
//   gpio_set_dir(BUTTON_PIN, GPIO_IN);
//   gpio_pull_up(BUTTON_PIN);
//
//   while (true) {
//     std::cout << gpio_get(BUTTON_PIN) << std::endl;
//     sleep_ms(500);
//   }
// }

// pins
#define I2C_1_SDA_PIN 10
#define I2C_1_SDL_PIN 11
#define LED_PIN 13
#define BUTTON_PIN 14
#define PCF8575_INTERRUPT_PIN 22

#define LED_LENGTH 8
#define PCF8575_ADDR 0x20
constexpr auto led_format = PicoLed::FORMAT_GRB;

// Debounce debounce(BUTTON_PIN, 4);
Debounce_PCF8575 d16(i2c1, 0x20, 500);

#define FPS 60
#define MS_PER_FRAME 16

#ifdef MEASURE_BOUNCE_TIMES
uint32_t times[256][2];
uint32_t time_idx = 0;
void sample_callback(uint gpio, uint32_t events) {
  if (time_idx >= 256) {
    return;
  }
  if (gpio == BUTTON_PIN) {
    if (events & GPIO_IRQ_EDGE_RISE) {
      times[time_idx][0] = time_us_32();
      times[time_idx][1] = 1;
    }
    if (events & GPIO_IRQ_EDGE_FALL) {
      times[time_idx][0] = time_us_32();
      times[time_idx][1] = 0;
    }
    ++time_idx;
  }
}
#endif

struct State {
  bool button = false;
  uint32_t tick = 0;
  PicoLed::Color button_color[LED_LENGTH];
};

float led_hues[LED_LENGTH];

State state;
volatile bool state_changed = true;
volatile bool pcf8575_changed = true;

auto read_pcf8575(i2c_inst_t *i2c, uint8_t addr) -> uint16_t {
  int ret;
  uint8_t rxdata[2];
  ret = i2c_read_timeout_us(i2c, addr, rxdata, 2, true, 5 * 1000);
  if (ret != 2) {
    std::cerr << "read pcf8575: unexpected return value " << ret << std::endl;
  }
  return *reinterpret_cast<uint16_t *>(rxdata);
}

void gpio_callback(uint gpio, uint32_t events) {
  // if (gpio == BUTTON_PIN)
  //   debounce.on_event(events);
}

void pcf8575_callback(uint gpio, uint32_t events) {
  d16.on_pcf8575_interrupt();
}

int64_t on_frame(alarm_id_t id, void *user_data) {
  // std::cout << "on_frame " << state.tick << " "
  //           << to_ms_since_boot(get_absolute_time()) << std::endl;
  // state.button = debounce.state();

  auto constexpr duration_sec = 2;
  auto constexpr duration_frames = duration_sec * FPS;
  auto constexpr duration_frames_2 = duration_sec * FPS / 2;

  auto f = state.tick % duration_frames;
  if (f > duration_frames_2)
    f = duration_frames - f;
  uint8_t v = 32 + 128 * (f / static_cast<float>(duration_frames_2));

  if (state.tick % (2 * FPS) == 0) {
    std::rotate(led_hues, led_hues + 1, led_hues + LED_LENGTH);
  }

  if (state.button) {
    for (int i = 0; i < LED_LENGTH; ++i) {
      float h = led_hues[i];
      auto const [r, g, b] = hsv_to_rgb(h, 1.0, v);
      state.button_color[i] = PicoLed::RGB(r, g, b);
    }
  } else {
    for (int i = 0; i < LED_LENGTH; ++i) {
      float h = led_hues[i];
      auto const [r, g, b] = hsv_to_rgb(h, 1.0, 1.0);
      state.button_color[i] = PicoLed::RGB(r, g, b);
    }
  }

  ++state.tick;
  state_changed = true;
  return MS_PER_FRAME * 1000; // microseconds
}

int main() {
  stdio_init_all();

  i2c_init(i2c1, 100 * 1000);
  gpio_set_function(I2C_1_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_1_SDL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_1_SDA_PIN);
  gpio_pull_up(I2C_1_SDL_PIN);

  gpio_init(PCF8575_INTERRUPT_PIN);
  gpio_set_dir(PCF8575_INTERRUPT_PIN, GPIO_IN);
  gpio_pull_up(PCF8575_INTERRUPT_PIN);
  gpio_set_irq_enabled_with_callback(PCF8575_INTERRUPT_PIN,
                                     GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                     true, &pcf8575_callback);

  for (int i = 0; i < LED_LENGTH; ++i) {
    led_hues[i] = i / static_cast<float>(LED_LENGTH) * 360.f;
  }

#ifdef MEASURE_BOUNCE_TIMES
  for (auto i = 0; i < 256; ++i) {
    for (auto j = 0; j < 2; ++j) {
      times[i][j] = 0;
    }
  }
#endif

  gpio_init(BUTTON_PIN);
  gpio_set_dir(BUTTON_PIN, GPIO_IN);
  gpio_pull_up(BUTTON_PIN);
#ifdef MEASURE_BOUNCE_TIMES
  gpio_set_irq_enabled_with_callback(BUTTON_PIN,
                                     GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
                                     true, &sample_callback);
#else
  // gpio_set_irq_enabled_with_callback(BUTTON_PIN,
  //                                    GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
  //                                    true, &gpio_callback);

#if 0
  auto ledStrip = PicoLed::addLeds<PicoLed::WS2812B>(
      pio0, 0, LED_PIN, LED_LENGTH, led_format);
  ledStrip.setBrightness(255);
  ledStrip.clear();

  ledStrip.fill(PicoLed::RGB(255, 0, 0));
  ledStrip.show();

  bool printed = false;

  std::cout << "START" << std::endl;

  volatile bool last_state = true;

  add_alarm_in_ms(MS_PER_FRAME, on_frame, nullptr, false);
#endif

  sleep_ms(100);
#if 1
  std::cout << "IO expander initialisation" << std::endl;
  for (int i = 0; i < 3; ++i) {
    uint8_t rxdata[2] = {0xff, 0xff};
    auto ret = i2c_write_blocking(i2c1, PCF8575_ADDR, rxdata, 2, false);
    std::cout << "  try " << i << " : RET = " << ret << std::endl;
    // if (ret == PICO_ERROR_GENERIC) {
    //   std::cout << "generic = " << ret << std::endl;
    // }
    sleep_ms(100);
    auto pins = read_pcf8575(i2c1, PCF8575_ADDR);
    std::cout << "  IO expander says: " << std::bitset<16>(pins) << std::endl;
    sleep_ms(100);
  }
  std::cout << "IO expander init done." << std::endl;
#endif

  while (true) {
#if MEASURE_BOUNCE_TIMES
    if (!printed && time_us_32() >= 10 * 1000 * 1000) {
      for (auto i = 0; i <= time_idx; ++i) {
        for (auto j = 0; j < 2; ++j) {
          std::cout << times[i][j] << " ";
        }
        std::cout << std::endl;
      }
      printed = true;
    }
#endif

    // if (debounce.state() != last_state) {
    //    last_state = debounce.state();
    //    std::cout << "button state = " << last_state << std::endl;
    // }

    // std::cout << gpio_get(BUTTON_PIN) << std::endl;
    // sleep_ms(500);

    //   if (last_state) {
    //     ledStrip.fill(PicoLed::RGB(255, 0, 0));
    //     ledStrip.show();
    //   }
    //   else {
    //     ledStrip.fill(PicoLed::RGB(0, 0, 255));
    //     ledStrip.show();
    //   }
    // }
    //

    if (d16.loop()) {
      std::cout << "pcf8575_changed!" << std::endl;
      std::cout << "IO expander says: " << std::bitset<16>(d16.state())
                << std::endl;
    }

#if 0
    if (state_changed) {
      state_changed = false;
      for (int i = 0; i < LED_LENGTH; ++i) {
        ledStrip.setPixelColor(i, state.button_color[i]);
      }
      ledStrip.show();
    }
#endif
#endif
}

return 0;
}
