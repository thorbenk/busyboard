#include "pico/stdlib.h"
#include <algorithm>
#include <iostream>
#include <stdio.h>

#include <PicoLed.hpp>

#include "debounce.h"

#define LED_PIN 13
#define LED_LENGTH 8
#define BUTTON_PIN 12

Debounce debounce(BUTTON_PIN, 4);

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

struct State {
  bool button = false;
  uint32_t tick = 0;
  PicoLed::Color button_color[8];
};

float led_hues[8];

State state;
volatile bool state_changed = true;

void gpio_callback(uint gpio, uint32_t events) {
  if (gpio == BUTTON_PIN)
    debounce.on_event(events);
}

int64_t on_frame(alarm_id_t id, void *user_data) {
  std::cout << "on_frame " << state.tick << " "
            << to_ms_since_boot(get_absolute_time()) << std::endl;
  state.button = debounce.state();

  auto constexpr duration_sec = 2;
  auto constexpr duration_frames = duration_sec * FPS;
  auto constexpr duration_frames_2 = duration_sec * FPS / 2;

  auto f = state.tick % duration_frames;
  if (f > duration_frames_2)
    f = duration_frames - f;
  uint8_t v = 32 + 128 * (f / static_cast<float>(duration_frames_2));

  if (state.tick % (2 * FPS) == 0) {
    std::rotate(led_hues, led_hues + 1, led_hues + 8);
  }

  if (state.button) {
    for (int i = 0; i < 8; ++i) {
      float h = led_hues[i];
      auto const [r, g, b] = hsv_to_rgb(h, 1.0, v);
      state.button_color[i] = PicoLed::RGB(r, g, b);
    }
  } else {
    for (int i = 0; i < 8; ++i) {
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

  for (int i = 0; i < 8; ++i) {
    led_hues[i] = i / 8.f * 360.f;
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
  gpio_set_irq_enabled_with_callback(BUTTON_PIN,
                                     GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
                                     true, &gpio_callback);

  auto ledStrip = PicoLed::addLeds<PicoLed::WS2812B>(
      pio0, 0, LED_PIN, LED_LENGTH, PicoLed::FORMAT_GRB);
  ledStrip.setBrightness(255);
  ledStrip.clear();

  ledStrip.fill(PicoLed::RGB(255, 0, 0));
  ledStrip.show();

  bool printed = false;

  std::cout << "START" << std::endl;

  volatile bool last_state = true;

  add_alarm_in_ms(MS_PER_FRAME, on_frame, nullptr, false);

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
    //   last_state = debounce.state();
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

    if (state_changed) {
      state_changed = false;
      for (int i = 0; i < 8; ++i) {
        ledStrip.setPixelColor(i, state.button_color[i]);
      }
      ledStrip.show();
    }
#endif
}

return 0;
}
