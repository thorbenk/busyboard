#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <pico/stdlib.h>

#include <bitset>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <PicoLed.hpp>
#include <pico7219/pico7219.h>

#include "color.h"
#include "debounce.h"
#include "dfPlayerDriver.h"

// hardware ------------------------------------------------------------------
// - Raspberry Pi Pico
// - IO expander PCF8575
// - DF Player Mini

// pins ----------------------------------------------------------------------
//
// GP  0 - stdio TX
// GP  1 - stdio RX
// GP  2 - I2C1 SDA
// GP  3 - I2C1 SCL
// GP  4
// GP  5
// GP  6
// GP  8 - UART1 TX -> DF Player Mini
// GP  9 - UART1 TX -> DF Player Mini
// GP 10 - SPI1 SCK -> Dot matrix CLOCK
// GP 11 - SPI1 TX  -> Dot matrix DIN
// GP 12 - <reserved for SPI1 RX?>
// GP 13 - SPI1 CSn -> Dot matrix CS
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
#define IO_EXPAND_16_DEVICE_1_DEBOUNCE_MSEC 2
#define ARCADE_BUTTONS_8_DIN_PIN 16
#define ARCADE_BUTTONS_8_LED_LENGTH 8
#define FAN_LED_LENGTH 6
constexpr auto arcade_buttons_8_led_format = PicoLed::FORMAT_GRB;
constexpr auto fan_led_format = PicoLed::FORMAT_GRB;
static_assert(arcade_buttons_8_led_format == fan_led_format);
constexpr auto grb_led_string_length =
    ARCADE_BUTTONS_8_LED_LENGTH + FAN_LED_LENGTH;
#define DOT_MATRIX_SPI_CHAN PicoSpiNum::PICO_SPI_1
#define DOT_MATRIX_SPI_SCK 10
#define DOT_MATRIX_SPI_TX 11
#define DOT_MATRIX_SPI_CS 13
#define DOT_MATRIX_SPI_BAUDRATE 1500 * 1000
#define DOT_MATRIX_CHAIN_LEN 4

#define DFPLAYER_UART 1
#define DFPLAYER_MINI_RX 8 /* GP 8 - TX (UART 1) */
#define DFPLAYER_MINI_TX 9 /* GP 9 - RX (UART 1) */

#define FPS 60
#define MS_PER_FRAME 16

enum class FaderMode { A, B, C };

//----------------------------------------------------------------------------
// globals
//----------------------------------------------------------------------------

Debounce_PCF8575 io16_dev1(IO_EXPAND_16_DEVICE_1_I2C_LANE,
                           IO_EXPAND_16_DEVICE_1_I2C_ADDRESS,
                           IO_EXPAND_16_DEVICE_1_DEBOUNCE_MSEC);

volatile bool io16_device1_changed = true;
std::optional<uint16_t> io16_device1_prev_state;

struct State {
  uint8_t buttons_8 = 0;
  uint32_t tick = 0;
  PicoLed::Color grb_led_string[grb_led_string_length];
  FaderMode fader_mode = FaderMode::A;
  bool toggle_upper_left = false;
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

  float hue_on = 120.0;
  float hue_off = 0.0;

  if (state.fader_mode == FaderMode::A) {
    hue_on = 120.0;
    hue_off = 0.0;
  } else if (state.fader_mode == FaderMode::B) {
    hue_on = 60.0;
    hue_off = 180.0;
  } else if (state.fader_mode == FaderMode::C) {
    hue_on = 200.0;
    hue_off = 300.0;
  }

  for (int i = 0; i < ARCADE_BUTTONS_8_LED_LENGTH; ++i) {
    float v_arcade = v;
    if (!state.toggle_upper_left)
      v_arcade = 0.f;

    if ((1 << i) & state.buttons_8) {
      // button pressed
      auto const [r, g, b] = hsv_to_rgb(hue_on, 1.0, v_arcade);
      state.grb_led_string[i] = PicoLed::RGB(r, g, b);
    } else {
      // button not pressed
      auto const [r, g, b] = hsv_to_rgb(hue_off, 1.0, v_arcade);
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

//---------------------------------------------------------------------------

extern uint8_t console_font_8x8[];

// Draw a character to the library. Note that the width of the "virtual
// display" can be much longer than the physical module chain, and
// off-display elements can later be scrolled into view. However, it's
// the job of the application, not the library, to size the virtual
// display sufficiently to fit all the text in.
//
// chr is an offset in the font table, which starts with character 32 (space).
// It isn't an ASCII character.
void draw_character(Pico7219 *pico7219, uint8_t chr, int x_offset, BOOL flush) {
  for (int i = 0; i < 8; i++) // row
  {
    // The font elements are one byte wide even though, as its an 8x5 font,
    //   only the top five bits of each byte are used.
    uint8_t v = console_font_8x8[8 * chr + i];
    for (int j = 0; j < 8; j++) // column
    {
      int sel = 1 << j;
      if (sel & v)
        pico7219_switch_on(pico7219, 7 - i, 7 - j + x_offset, FALSE);
    }
  }
  if (flush)
    pico7219_flush(pico7219);
}

// Draw a string of text on the (virtual) display. This function assumes
// that the library has already been configured to provide a virtual
// chain of LED modules that is long enough to fit all the text onto.
void draw_string(Pico7219 *pico7219, const char *s, BOOL flush) {
  int x = 0;
  while (*s) {
    draw_character(pico7219, *s, x, FALSE);
    s++;
    x += 8;
  }
  if (flush)
    pico7219_flush(pico7219);
}

// Get the number of horizontal pixels that a string will take. Since each
// font element is five pixels wide, and there is one pixel between each
// character, we just multiply the string length by 6.
int get_string_length_pixels(const char *s) { return std::strlen(s) * 6; }

// Get the number of 8x8 LED modules that would be needed to accomodate the
// string of text. That's the number of pixels divided by 8 (the module
// width), and then one added to round up.
int get_string_length_modules(const char *s) {
  return get_string_length_pixels(s) / 8 + 1;
}

// Show a string of characters, and then scroll it across the display.
// This function uses pico7219_set_virtual_chain_length() to ensure that
// there are enough "virtual" modules in the display chain to fit
// the whole string. It then scrolls it enough times to scroll the
// whole string right off the end.
void show_text_and_scroll(Pico7219 *pico7219, const char *string) {
  pico7219_set_virtual_chain_length(pico7219,
                                    get_string_length_modules(string));
  draw_string(pico7219, string, FALSE);
  pico7219_flush(pico7219);

  int l = get_string_length_pixels(string);

  for (int i = 0; i < l; i++) {
    sleep_ms(50);
    pico7219_scroll(pico7219, FALSE);
  }

  pico7219_switch_off_all(pico7219, TRUE);
  sleep_ms(500);
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

  Pico7219 *dot_matrix = pico7219_create(
      DOT_MATRIX_SPI_CHAN, DOT_MATRIX_SPI_BAUDRATE, DOT_MATRIX_SPI_TX,
      DOT_MATRIX_SPI_SCK, DOT_MATRIX_SPI_CS, DOT_MATRIX_CHAIN_LEN, false);

  pico7219_switch_off_all(dot_matrix, false);

  ledStrip.setBrightness(255);
  ledStrip.clear();
  ledStrip.fill(PicoLed::RGB(0, 0, 64));
  ledStrip.show();

  draw_string(dot_matrix, "LUAN", false);
  pico7219_set_intensity(dot_matrix, 0);
  pico7219_flush(dot_matrix);

  uint32_t frame_usec_min = 0;
  uint32_t frame_usec_max = 0;

  DfPlayerPico<DFPLAYER_UART, DFPLAYER_MINI_TX, DFPLAYER_MINI_RX> dfp;
  dfp.reset();
  sleep_ms(2000);
  dfp.specifyVolume(10);
  sleep_ms(200);

  {
    uint8_t num = 3;
    uint8_t folder = 1;
    uint8_t track = static_cast<uint8_t>(num);
    uint16_t cmd = (folder << 8) | num;
    dfp.sendCmd(dfPlayer::SPECIFY_FOLDER_PLAYBACK, cmd);
  }

  io16_dev1.init();

  bool arcade8_num_changed = false;

  while (true) {
    if (io16_dev1.loop()) {
      std::cout << "io16 changed" << std::endl;
      auto const current_state = io16_dev1.state();
      for (auto i = 0; i < 16; ++i) {
        bool const current = ((1 << i) & current_state) > 0;
        bool const prev = io16_device1_prev_state.has_value()
                              ? (((1 << i) & *io16_device1_prev_state) > 0)
                              : (!current);
        if (prev != current) {
          std::cout << "button " << i << " : " << (int)prev << " -> "
                    << (int)current << std::endl;
        }
        if (i < 8 && prev == 1 && current == 0) {
          arcade8_num_changed = true;
          // this means the button was pressed down.
          state.buttons_8 ^= (1 << i);
          std::cout << "button 8 = " << (int)state.buttons_8 << std::endl;
        }
        if (i == 8 && prev == 1 && current == 0) {
          std::cout << "fader push A" << std::endl;
          state.fader_mode = FaderMode::A;
        } else if (i == 9 && prev == 1 && current == 0) {
          std::cout << "fader push B" << std::endl;
          state.fader_mode = FaderMode::B;
        } else if (i == 10 && prev == 1 && current == 0) {
          std::cout << "fader push C" << std::endl;
          state.fader_mode = FaderMode::C;
        } else if (i == 11) {
          state.toggle_upper_left = (current == 1);
        }
      }
      io16_device1_prev_state = io16_dev1.state();
    }

    if (frame_changed) {
      auto start = time_us_32();
      calc_frame();
      frame_changed = false;
      for (int i = 0; i < grb_led_string_length; ++i) {
        ledStrip.setPixelColor(i, state.grb_led_string[i]);
      }
      ledStrip.show();
      if (arcade8_num_changed) {
        std::cout << "update dot matrix" << std::endl;
        char b[4] = {0, 0, 0, 0};
        char b2[4] = {' ', ' ', ' ', 0};
        itoa(state.buttons_8, b, 10);
        if (state.buttons_8 >= 100)
          std::copy(b, b + 3, b2 + 1);
        else if (state.buttons_8 >= 10)
          std::copy(b, b + 2, b2 + 2);
        else
          std::copy(b, b + 2, b2 + 3);

        pico7219_switch_off_all(dot_matrix, false);
        draw_string(dot_matrix, b2, false);
        pico7219_flush(dot_matrix);

        arcade8_num_changed = false;
      }
      auto end = time_us_32();

      // debug frames per second
      auto dur = end - start;
      frame_usec_max = std::max(frame_usec_max, dur);
      frame_usec_min = std::min(frame_usec_min, dur);
      if (state.tick % FPS == 0) {
        std::cout << "frame time min=" << frame_usec_min
                  << ", max=" << frame_usec_max << " usec" << std::endl;
      }
    }
  }

  return 0;
}
