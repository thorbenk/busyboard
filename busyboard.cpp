#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <pico/stdlib.h>

#include <bitset>
#include <iostream>

#include <PicoLed.hpp>

#include "color.h"
#include "debounce.h"

// hardware ------------------------------------------------------------------
// - Raspberry Pi Pico
// - IO expander PCF8575

// pins ----------------------------------------------------------------------
//
// GP  0 - stdio TX
// GP  1 - stdio RX
// GP  2 - I2C1 SDA
// GP  3 - I2C1 SCL
// GP  4
// GP  5
// GP  6
// GP  8
// GP  9
// GP 10
// GP 11
// GP 12
// GP 13
// GP 14
// GP 15
//
// GP 16 - data in for WS2812b: string of 8 arcade buttons and 6 LEDs in fan
// GP 17
// GP 18
// GP 19
// GP 20
// GP 21
// GP 22
// GP 23
// GP 24
// GP 25
// GP 26
// GP 27
// GP 28 - IO expander 1 interrupt
//----------------------------------------------------------------------------

#define I2C_1_SDA_PIN 2
#define I2C_1_SDL_PIN 3
#define I2C_1_BAUD_RATE 100 * 1000
#define IO_EXPAND_16_DEVICE_1_I2C_LANE i2c1
#define IO_EXPAND_16_DEVICE_1_INTERRUPT_PIN 28
#define IO_EXPAND_16_DEVICE_1_I2C_ADDRESS 0x20
#define IO_EXPAND_16_DEVICE_1_DEBOUNCE_MSEC 10
#define ARCADE_BUTTONS_8_DIN_PIN 16
#define ARCADE_BUTTONS_8_LED_LENGTH 8
#define FAN_LED_LENGTH 6
constexpr auto arcade_buttons_8_led_format = PicoLed::FORMAT_GRB;
constexpr auto fan_led_format = PicoLed::FORMAT_GRB;
static_assert(arcade_buttons_8_led_format == fan_led_format);
constexpr auto grb_led_string_length =
    ARCADE_BUTTONS_8_LED_LENGTH + FAN_LED_LENGTH;

#define FPS 60
#define MS_PER_FRAME 16

//----------------------------------------------------------------------------
// globals
//----------------------------------------------------------------------------

Debounce_PCF8575 io16_dev1(IO_EXPAND_16_DEVICE_1_I2C_LANE,
                           IO_EXPAND_16_DEVICE_1_I2C_ADDRESS,
                           IO_EXPAND_16_DEVICE_1_DEBOUNCE_MSEC);

volatile bool io16_device1_changed = true;
uint16_t io16_device1_prev_state = 0;

struct State {
  uint8_t buttons_8 = 0;
  uint32_t tick = 0;
  PicoLed::Color grb_led_string[grb_led_string_length];
};

State state;
volatile bool frame_changed = true;

//----------------------------------------------------------------------------

auto read_pcf8575(i2c_inst_t *i2c, uint8_t addr) -> uint16_t {
  int ret;
  uint8_t rxdata[2];
  ret = i2c_read_timeout_us(i2c, addr, rxdata, 2, true, 5 * 1000);
  if (ret != 2) {
    std::cerr << "read pcf8575: unexpected return value " << ret << std::endl;
  }
  return *reinterpret_cast<uint16_t *>(rxdata);
}

void io_expand16_device1_callback(uint gpio, uint32_t events) {
  io16_dev1.on_pcf8575_interrupt();
}

auto calc_frame() -> void {
  auto constexpr duration_sec = 2;
  auto constexpr duration_frames = duration_sec * FPS;
  auto constexpr duration_frames_2 = duration_sec * FPS / 2;
  auto constexpr N = ARCADE_BUTTONS_8_LED_LENGTH;

  auto f = state.tick % duration_frames;
  if (f > duration_frames_2)
    f = duration_frames - f;
  uint8_t v = 32 + 128 * (f / static_cast<float>(duration_frames_2));

  for (int i = 0; i < ARCADE_BUTTONS_8_LED_LENGTH; ++i) {
    if ((1 << i) & state.buttons_8) {
      // button pressed: green
      auto const [r, g, b] = hsv_to_rgb(120.0, 1.0, v);
      state.grb_led_string[i] = PicoLed::RGB(r, g, b);
    } else {
      // button not pressed: red
      auto const [r, g, b] = hsv_to_rgb(0.0, 1.0, v);
      state.grb_led_string[i] = PicoLed::RGB(r, g, b);
    }
  }

  for (int i = ARCADE_BUTTONS_8_LED_LENGTH; i < grb_led_string_length; ++i) {
    auto const [r, g, b] = hsv_to_rgb(220.0, 1.0, v);
    state.grb_led_string[i] = PicoLed::RGB(r, g, b);
  }
}

int64_t on_frame(alarm_id_t id, void *user_data) {
  ++state.tick;
  frame_changed = true;
  return MS_PER_FRAME * 1000; // microseconds
}

//----------------------------------------------------------------------------

int main() {
  stdio_init_all();

  i2c_init(i2c1, I2C_1_BAUD_RATE);
  gpio_set_function(I2C_1_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_1_SDL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_1_SDA_PIN);
  gpio_pull_up(I2C_1_SDL_PIN);

  gpio_init(IO_EXPAND_16_DEVICE_1_INTERRUPT_PIN);
  gpio_set_dir(IO_EXPAND_16_DEVICE_1_INTERRUPT_PIN, GPIO_IN);
  gpio_pull_up(IO_EXPAND_16_DEVICE_1_INTERRUPT_PIN);
  gpio_set_irq_enabled_with_callback(IO_EXPAND_16_DEVICE_1_INTERRUPT_PIN,
                                     GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                     true, &io_expand16_device1_callback);

  add_alarm_in_ms(MS_PER_FRAME, on_frame, nullptr, false);

  auto ledStrip = PicoLed::addLeds<PicoLed::WS2812B>(
      pio0, 0, ARCADE_BUTTONS_8_DIN_PIN, grb_led_string_length,
      arcade_buttons_8_led_format);

  ledStrip.setBrightness(255);
  ledStrip.clear();
  ledStrip.fill(PicoLed::RGB(0, 255, 255));
  ledStrip.show();

  sleep_ms(100);

  io16_dev1.init();
  io16_device1_prev_state = io16_dev1.state();

  while (true) {
    if (io16_dev1.loop()) {
      auto const current_state = io16_dev1.state();
      for (auto i = 0; i < 16; ++i) {
        bool prev = ((1 << i) & io16_device1_prev_state) > 0;
        bool current = ((1 << i) & current_state) > 0;
        if (prev != current) {
          std::cout << "button " << i << " : " << (int)prev << " -> "
                    << (int)current << std::endl;
        }
        if (i < 8 && prev == 1 && current == 0) {
          // this means the button was pressed down.
          state.buttons_8 ^= (1 << i);
          std::cout << "button 8 = " << (int)state.buttons_8 << std::endl;
        }
      }
      io16_device1_prev_state = io16_dev1.state();
    }

    if (frame_changed) {
      calc_frame();
      frame_changed = false;
      for (int i = 0; i < grb_led_string_length; ++i) {
        ledStrip.setPixelColor(i, state.grb_led_string[i]);
      }
      ledStrip.show();
    }
  }

  return 0;
}
