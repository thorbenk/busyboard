#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <pico/stdlib.h>

#include <bitset>
#include <iostream>

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
// GP 16
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

//----------------------------------------------------------------------------
// globals
//----------------------------------------------------------------------------

Debounce_PCF8575 io16_dev1(IO_EXPAND_16_DEVICE_1_I2C_LANE,
                           IO_EXPAND_16_DEVICE_1_I2C_ADDRESS,
                           IO_EXPAND_16_DEVICE_1_DEBOUNCE_MSEC);

volatile bool io16_device1_changed = true;
uint16_t io16_device1_prev_state = 0;

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

  sleep_ms(100);

  io16_dev1.init();
  io16_device1_prev_state = io16_dev1.state();

#if 0
  std::cout << "IO expander initialisation" << std::endl;
  for (int i = 0; i < 3; ++i) {
    uint8_t rxdata[2] = {0xff, 0xff};
    auto ret = i2c_write_blocking(i2c1, IO_EXPAND_16_DEVICE_1_I2C_ADDRESS,
                                  rxdata, 2, false);
    std::cout << "  try " << i << " : RET = " << ret << std::endl;
    // if (ret == PICO_ERROR_GENERIC) {
    //   std::cout << "generic = " << ret << std::endl;
    // }
    sleep_ms(100);
    auto pins = read_pcf8575(i2c1, IO_EXPAND_16_DEVICE_1_I2C_ADDRESS);
    std::cout << "  IO expander says: " << std::bitset<16>(pins) << std::endl;
    sleep_ms(100);
  }
  std::cout << "IO expander init done." << std::endl;
#endif

  while (true) {
    if (io16_dev1.loop()) {
      auto const state = io16_dev1.state();
      for (auto i = 0; i < 16; ++i) {
        bool prev = ((1 << i) & io16_device1_prev_state) > 0;
        bool now = ((1 << i) & state) > 0;
        if (prev != now) {
          std::cout << "button " << i << " : " << (int)prev << " -> "
                    << (int)now << std::endl;
        }
      }
      io16_device1_prev_state = io16_dev1.state();
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
  }

  return 0;
}
