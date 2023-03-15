// #define IO16_DEV2_ENABLED

#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include <pico/stdlib.h>

#include <bitset>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <PicoLed.hpp>
#include <pico7219/pico7219.h>
extern "C" {
#include "ads1115.h"
}

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
// GP  4 - data in for WS2812b: 3 WGRB (fader panel)
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
// GP 18 - phone dial: pulsed number (brown cable)
// GP 19
// GP 20
// GP 21 - fan PWM signal
// GP 22 - phone dial: dial in progress (green cable)
// GP 23
// GP 24
// GP 25
// GP 26 - data in for WS2812b: string of 9 WRGB for phone dial
// GP 27 - IO expander 2 interrupt
// GP 28 - IO expander 1 interrupt
//----------------------------------------------------------------------------

#define I2C_1_SDA_PIN 2
#define I2C_1_SDL_PIN 3
#define I2C_1_BAUD_RATE 100 * 1000
#define IO_EXPAND_16_DEVICE_1_I2C_LANE i2c1
#define IO_EXPAND_16_DEVICE_1_INTERRUPT_PIN 28
#define IO_EXPAND_16_DEVICE_1_I2C_ADDRESS 0x20
#define IO_EXPAND_16_DEVICE_1_DEBOUNCE_MSEC 2
#define IO_EXPAND_16_DEVICE_2_I2C_LANE i2c1
#define IO_EXPAND_16_DEVICE_2_INTERRUPT_PIN 27
#define IO_EXPAND_16_DEVICE_2_I2C_ADDRESS 0x23
#define IO_EXPAND_16_DEVICE_2_DEBOUNCE_MSEC 2
#define ARCADE_BUTTONS_8_DIN_PIN 16
#define ARCADE_BUTTONS_8_LED_LENGTH 8
#define FAN_LED_LENGTH 6
#define FAN_PWM_PIN 21
constexpr auto arcade_buttons_8_led_format = PicoLed::FORMAT_GRB;
constexpr auto fan_led_format = PicoLed::FORMAT_GRB;
static_assert(arcade_buttons_8_led_format == fan_led_format);
constexpr auto grb_led_string_length =
    ARCADE_BUTTONS_8_LED_LENGTH + FAN_LED_LENGTH;

#define FADER_LED_LENGTH 3
#define ANALOG_METER_LED_LENGTH 8
#define FADER_AND_ANALOG_METER_DIN_PIN 4
constexpr auto fader_led_format = PicoLed::FORMAT_WGRB;
constexpr auto analog_meter_led_format = PicoLed::FORMAT_WGRB;
static_assert(fader_led_format == analog_meter_led_format);
constexpr auto fader_and_analog_meter_led_string_length =
    FADER_LED_LENGTH + ANALOG_METER_LED_LENGTH;

#define PHONE_LEDS_DIN_PIN 26
#define PHONE_LEDS_LENGTH 9
constexpr auto phone_led_format = PicoLed::FORMAT_WGRB;

#define PHONE_DIAL_IN_PROGRESS_PIN 22
#define PHONE_DIAL_PULSED_NUMBER 18

#define DOT_MATRIX_SPI_CHAN PicoSpiNum::PICO_SPI_1
#define DOT_MATRIX_SPI_SCK 10
#define DOT_MATRIX_SPI_TX 11
#define DOT_MATRIX_SPI_CS 13
#define DOT_MATRIX_SPI_BAUDRATE 1500 * 1000
#define DOT_MATRIX_CHAIN_LEN 4

#define DFPLAYER_UART 1
#define DFPLAYER_MINI_RX 8 /* GP 8 - TX (UART 1) */
#define DFPLAYER_MINI_TX 9 /* GP 9 - RX (UART 1) */

#define ADC_I2C_ADDR 0x48
#define ADC_I2C_PORT i2c1

#define FPS 60
#define MS_PER_FRAME 16

enum class FaderMode { RGB, HSV, Effect };

enum class ArcadeMode { Binary, Names };

constexpr uint16_t fader_min_max[4][2]{
    {72, 19813}, {64, 19707}, {121, 19657}, {84, 19787}};

//----------------------------------------------------------------------------
// globals
//----------------------------------------------------------------------------

Debounce_PCF8575 io16_dev1(IO_EXPAND_16_DEVICE_1_I2C_LANE,
                           IO_EXPAND_16_DEVICE_1_I2C_ADDRESS,
                           IO_EXPAND_16_DEVICE_1_DEBOUNCE_MSEC);

#ifdef IO16_DEV2_ENABLED
Debounce_PCF8575 io16_dev2(IO_EXPAND_16_DEVICE_2_I2C_LANE,
                           IO_EXPAND_16_DEVICE_2_I2C_ADDRESS,
                           IO_EXPAND_16_DEVICE_2_DEBOUNCE_MSEC);
#endif

// DebounceEdge phone_pulse(5);
DebounceEdge phone_dialing_in_progress(50);

struct ads1115_adc adc;

volatile bool io16_device1_changed = true;
std::optional<uint16_t> io16_device1_prev_state;

struct State {
  uint8_t buttons_8 = 0;
  uint32_t tick = 0;
  PicoLed::Color grb_led_string[grb_led_string_length];
  PicoLed::Color fader_analog_string[fader_and_analog_meter_led_string_length];
  PicoLed::Color phone_leds[PHONE_LEDS_LENGTH];
  FaderMode fader_mode = FaderMode::RGB;
  ArcadeMode arcade_mode = ArcadeMode::Binary;
  bool toggle_upper_left = true; // FIXME
  uint8_t faders[4] = {0, 0, 0, 0};
  uint8_t fader_adc = 0;
  bool dial_in_progress = false;
  int8_t phone_dialed_num = -1;
  int8_t switch6 = 0;
  bool scroll_dotmatrix = false;
};

State state;
volatile bool frame_changed = true;

//----------------------------------------------------------------------------

void gpio_interrupt(uint gpio, uint32_t events) {
  if (gpio == PHONE_DIAL_PULSED_NUMBER) {
    // phone_pulse.on_event(events);
  } else if (gpio == PHONE_DIAL_IN_PROGRESS_PIN) {
    phone_dialing_in_progress.on_event(events);
  } else if (gpio == IO_EXPAND_16_DEVICE_1_INTERRUPT_PIN) {
    io16_dev1.on_pcf8575_interrupt();
  }
#ifdef IO16_DEV2_ENABLED
  else if (gpio == IO_EXPAND_16_DEVICE_2_INTERRUPT_PIN) {
    io16_dev2.on_pcf8575_interrupt();
  }
#endif
}

auto read_adc() -> void {
  constexpr ads1115_mux_t channels[4] = {
      ADS1115_MUX_SINGLE_0, ADS1115_MUX_SINGLE_1, ADS1115_MUX_SINGLE_2,
      ADS1115_MUX_SINGLE_3};

  uint16_t fader_raw;
  ads1115_read_adc(&fader_raw, &adc);

  uint16_t const m = fader_min_max[state.fader_adc][0];
  uint16_t const M = fader_min_max[state.fader_adc][1];

  // clip
  fader_raw = std::max(m, fader_raw);
  fader_raw = std::min(M, fader_raw);

  float f = fader_raw / static_cast<float>(M - m);
  uint8_t f8 = 255 - 255 * f;

  state.faders[state.fader_adc] = f8;

  // setup the ADC for next frame
  state.fader_adc = state.tick % 4;
  ads1115_set_operating_mode(ADS1115_MODE_CONTINUOUS, &adc);
  ads1115_set_input_mux(channels[state.fader_adc], &adc);
  ads1115_set_pga(ADS1115_PGA_4_096, &adc);
  ads1115_set_data_rate(ADS1115_RATE_860_SPS, &adc);
  ads1115_write_config(&adc);
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

  if (state.fader_mode == FaderMode::RGB) {
    hue_on = 120.0;
    hue_off = 0.0;
    state.fader_analog_string[0] = PicoLed::RGB(128, 0, 0);
    state.fader_analog_string[1] = PicoLed::RGB(0, 0, 0);
    state.fader_analog_string[2] = PicoLed::RGB(0, 0, 0);
  } else if (state.fader_mode == FaderMode::HSV) {
    hue_on = 60.0;
    hue_off = 180.0;
    state.fader_analog_string[0] = PicoLed::RGB(0, 0, 0);
    state.fader_analog_string[1] = PicoLed::RGB(0, 128, 0);
    state.fader_analog_string[2] = PicoLed::RGB(0, 0, 0);
  } else if (state.fader_mode == FaderMode::Effect) {
    hue_on = 200.0;
    hue_off = 300.0;
    state.fader_analog_string[0] = PicoLed::RGB(0, 0, 0);
    state.fader_analog_string[1] = PicoLed::RGB(0, 0, 0);
    state.fader_analog_string[2] = PicoLed::RGB(0, 0, 128);
  }

  // Fan speed
  // Lowest speed should be 10%
  // The PWM is inverted:
  uint8_t const fan_pwm =
      state.faders[0] < 25
          ? 0
          : std::max(static_cast<uint8_t>(25), state.faders[0]);
  std::cout << "FAN " << (int)fan_pwm << std::endl;
  pwm_set_gpio_level(FAN_PWM_PIN, fan_pwm);

  //
  // arcade buttons RGB lights
  //
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

  //
  // fan RGB lights
  //
  for (int i = ARCADE_BUTTONS_8_LED_LENGTH; i < grb_led_string_length; ++i) {
    if (state.fader_mode == FaderMode::RGB) {
      state.grb_led_string[i] =
          PicoLed::RGB(state.faders[1], state.faders[2], state.faders[3]);
    } else if (state.fader_mode == FaderMode::HSV) {
      float h = 360.f * state.faders[1] / 255.0;
      float s = state.faders[2] / 255.0;
      float v = state.faders[3];
      auto const [r, g, b] = hsv_to_rgb(h, s, v);
      state.grb_led_string[i] = PicoLed::RGB(r, g, b);
    } else {
      float angle = 360.f * (state.tick % (2 * FPS)) / float(2 * FPS);
      auto j = i - ARCADE_BUTTONS_8_LED_LENGTH;
      float step = 360.f / float(FAN_LED_LENGTH);
      auto const [r, g, b] =
          hsv_to_rgb(std::fmod(angle + j * step, 360.f), 1.0, 255);
      state.grb_led_string[i] = PicoLed::RGB(r, g, b);
    }
  }

  if (state.dial_in_progress) {
    float angle = 360.f * (state.tick % (2 * FPS)) / float(2 * FPS);
    float step = 360.f / float(PHONE_LEDS_LENGTH);
    for (int i = 0; i < PHONE_LEDS_LENGTH; ++i) {
      auto const [r, g, b] =
          hsv_to_rgb(std::fmod(angle + i * step, 360.f), 1.0, 255);
      state.phone_leds[i] = PicoLed::RGB(r, g, b);
    }
  } else {
    if (state.phone_dialed_num >= 0) {
      for (int i = 0; i < PHONE_LEDS_LENGTH; ++i) {
        state.phone_leds[i] = i < state.phone_dialed_num
                                  ? PicoLed::RGB(0, 32, 0)
                                  : PicoLed::RGB(32, 0, 0);
      }
    } else {
      for (int i = 0; i < PHONE_LEDS_LENGTH; ++i) {
        state.phone_leds[i] = PicoLed::RGBW(0, 0, 0, 32);
      }
    }
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
int get_string_length_pixels(const char *s) { return std::strlen(s) * 8; }

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
}

//----------------------------------------------------------------------------

int main() {
  stdio_init_all();

  i2c_init(i2c1, I2C_1_BAUD_RATE);
  gpio_set_function(I2C_1_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_1_SDL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_1_SDA_PIN);
  gpio_pull_up(I2C_1_SDL_PIN);

  gpio_init(PHONE_DIAL_IN_PROGRESS_PIN);
  gpio_set_dir(PHONE_DIAL_IN_PROGRESS_PIN, GPIO_IN);
  gpio_pull_up(PHONE_DIAL_IN_PROGRESS_PIN);
  gpio_set_irq_enabled_with_callback(PHONE_DIAL_IN_PROGRESS_PIN,
                                     GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                     true, &gpio_interrupt);

  gpio_init(PHONE_DIAL_PULSED_NUMBER);
  gpio_set_dir(PHONE_DIAL_PULSED_NUMBER, GPIO_IN);
  gpio_pull_up(PHONE_DIAL_PULSED_NUMBER);
  gpio_set_irq_enabled_with_callback(PHONE_DIAL_PULSED_NUMBER,
                                     GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                     true, &gpio_interrupt);

  gpio_init(IO_EXPAND_16_DEVICE_1_INTERRUPT_PIN);
  gpio_set_dir(IO_EXPAND_16_DEVICE_1_INTERRUPT_PIN, GPIO_IN);
  gpio_pull_up(IO_EXPAND_16_DEVICE_1_INTERRUPT_PIN);
  gpio_set_irq_enabled_with_callback(IO_EXPAND_16_DEVICE_1_INTERRUPT_PIN,
                                     GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                     true, &gpio_interrupt);

#ifdef IO16_DEV2_ENABLED
  gpio_init(IO_EXPAND_16_DEVICE_2_INTERRUPT_PIN);
  gpio_set_dir(IO_EXPAND_16_DEVICE_2_INTERRUPT_PIN, GPIO_IN);
  gpio_pull_up(IO_EXPAND_16_DEVICE_2_INTERRUPT_PIN);
  gpio_set_irq_enabled_with_callback(IO_EXPAND_16_DEVICE_2_INTERRUPT_PIN,
                                     GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                     true, &io_expand16_device2_callback);
#endif

  gpio_set_function(FAN_PWM_PIN, GPIO_FUNC_PWM);
  uint const fan_pwm_slice_num = pwm_gpio_to_slice_num(FAN_PWM_PIN);
  pwm_config fan_pwm_config = pwm_get_default_config();
  pwm_set_clkdiv(fan_pwm_slice_num, 20.f);
  pwm_config_set_wrap(&fan_pwm_config, 255);
  pwm_init(fan_pwm_slice_num, &fan_pwm_config, true);

  ads1115_init(ADC_I2C_PORT, ADC_I2C_ADDR, &adc);
  ads1115_set_operating_mode(ADS1115_MODE_SINGLE_SHOT, &adc);
  ads1115_set_input_mux(ADS1115_MUX_SINGLE_0, &adc);
  ads1115_set_pga(ADS1115_PGA_4_096, &adc);
  ads1115_set_data_rate(ADS1115_RATE_860_SPS, &adc);
  ads1115_write_config(&adc);

  auto arcade_and_fan_leds = PicoLed::addLeds<PicoLed::WS2812B>(
      pio0, 0, ARCADE_BUTTONS_8_DIN_PIN, grb_led_string_length,
      arcade_buttons_8_led_format);

  auto fader_and_analog_meter_leds = PicoLed::addLeds<PicoLed::WS2812B>(
      pio0, 1, FADER_AND_ANALOG_METER_DIN_PIN,
      fader_and_analog_meter_led_string_length, fader_led_format);

  auto phone_leds = PicoLed::addLeds<PicoLed::WS2812B>(
      pio0, 2, PHONE_LEDS_DIN_PIN, PHONE_LEDS_LENGTH, phone_led_format);

  Pico7219 *dot_matrix = pico7219_create(
      DOT_MATRIX_SPI_CHAN, DOT_MATRIX_SPI_BAUDRATE, DOT_MATRIX_SPI_TX,
      DOT_MATRIX_SPI_SCK, DOT_MATRIX_SPI_CS, DOT_MATRIX_CHAIN_LEN, false);

  pico7219_switch_off_all(dot_matrix, false);

  arcade_and_fan_leds.setBrightness(255);
  arcade_and_fan_leds.clear();
  arcade_and_fan_leds.setPixelColor(0, PicoLed::RGB(64, 0, 0));
  arcade_and_fan_leds.show();

  fader_and_analog_meter_leds.setBrightness(255);
  fader_and_analog_meter_leds.clear();
  fader_and_analog_meter_leds.show();

  phone_leds.setBrightness(255);
  phone_leds.clear();
  phone_leds.fill(PicoLed::RGBW(0, 0, 0, 16));
  phone_leds.show();

  pico7219_set_intensity(dot_matrix, 0);
  pico7219_flush(dot_matrix);

  uint32_t frame_usec_min = 0;
  uint32_t frame_usec_max = 0;

  DfPlayerPico<DFPLAYER_UART, DFPLAYER_MINI_TX, DFPLAYER_MINI_RX> dfp;
  dfp.reset();
  sleep_ms(2000);
  dfp.specifyVolume(10);
  sleep_ms(200);

  io16_dev1.init();
#ifdef IO16_DEV2_ENABLED
  io16_dev2.init();
#endif

  bool arcade8_num_changed = false;

  add_alarm_in_ms(MS_PER_FRAME, on_frame, nullptr, false);

  int phone_dialing_prev = 0;

  uint32_t pulse_time[2] = {time_us_32(), time_us_32()};
  bool last_num_switch = false;
  int num = -1;
  int display_num = -1;
  int last_display_num = -1;

  bool switch6_changed = false;

  while (true) {
    {
      bool const dial_in_progress = !gpio_get(PHONE_DIAL_IN_PROGRESS_PIN);
      bool const num_switched = gpio_get(PHONE_DIAL_PULSED_NUMBER);

      pulse_time[num_switched] = time_us_32();

      if (dial_in_progress) {
        if (num == -1) {
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
          display_num = num;
          num = -1;
          last_num_switch = false;
          state.phone_dialed_num = display_num;
        }
      }
    }

    if (io16_dev1.loop()) {
      auto const current_state = io16_dev1.state();
      int8_t new_switch6 = 0;
      for (auto i = 0; i < 16; ++i) {
        bool const current = ((1 << i) & current_state) > 0;
        bool const prev = io16_device1_prev_state.has_value()
                              ? (((1 << i) & *io16_device1_prev_state) > 0)
                              : (!current);
        if (i < 8 && prev == 1 && current == 0) {
          arcade8_num_changed = true;
          // this means the button was pressed down.
          if (state.arcade_mode == ArcadeMode::Binary) {
            state.buttons_8 ^= (1 << i);
          } else {
            state.buttons_8 = (1 << i);
          }
        }
        if (i == 8 && prev == 1 && current == 0) {
          state.fader_mode = FaderMode::RGB;
        } else if (i == 9 && prev == 1 && current == 0) {
          state.fader_mode = FaderMode::HSV;
        } else if (i == 10 && prev == 1 && current == 0) {
          state.fader_mode = FaderMode::Effect;
          //} else if (i == 11) {
          //  state.toggle_upper_left = (current == 1);
        } else if (i >= 11 && i < 16) {
          if (current == 0) {
            // wiring mistakes where made
            if (i == 11 + 3) {
              new_switch6 = 1;
            } else if (i == 11 + 4) {
              new_switch6 = 2;
            } else if (i == 11 + 0) {
              new_switch6 = 3;
            } else if (i == 11 + 1) {
              new_switch6 = 4;
            } else if (i == 11 + 2) {
              new_switch6 = 5;
            }
          }
        }
      }
      if (state.switch6 != new_switch6) {
        state.switch6 = new_switch6;
        switch6_changed = true;
      }
      if (state.switch6 == 0) {
        state.arcade_mode = ArcadeMode::Binary;
      } else {
        state.arcade_mode = ArcadeMode::Names;
      }
      io16_device1_prev_state = io16_dev1.state();
    }
#ifdef IO16_DEV2_ENABLED
    if (io16_dev2.loop()) {
      std::cout << "io16 dev 2 changed" << std::endl;
    }
#endif

    if (frame_changed) {
      auto start = time_us_32();

      read_adc();

      calc_frame();
      frame_changed = false;

      if (state.scroll_dotmatrix && state.tick % 5 == 0) {
        pico7219_scroll(dot_matrix, true);
      }

      if (last_display_num != state.phone_dialed_num) {
        uint8_t folder = 1;
        uint8_t track = static_cast<uint8_t>(state.phone_dialed_num);
        uint16_t cmd = (folder << 8) | state.phone_dialed_num;
        dfp.sendCmd(dfPlayer::SPECIFY_FOLDER_PLAYBACK, cmd);
        last_display_num = state.phone_dialed_num;
      }

      for (int i = 0; i < grb_led_string_length; ++i) {
        arcade_and_fan_leds.setPixelColor(i, state.grb_led_string[i]);
      }
      arcade_and_fan_leds.show();

      for (int i = 0; i < fader_and_analog_meter_led_string_length; ++i) {
        fader_and_analog_meter_leds.setPixelColor(i,
                                                  state.fader_analog_string[i]);
      }
      fader_and_analog_meter_leds.show();

      for (int i = 0; i < PHONE_LEDS_LENGTH; ++i) {
        phone_leds.setPixelColor(i, state.phone_leds[i]);
      }
      phone_leds.show();

      if (arcade8_num_changed || switch6_changed) {
        std::cout << "update dot matrix" << std::endl;
        pico7219_switch_off_all(dot_matrix, false);
        if (state.arcade_mode == ArcadeMode::Binary) {
          char b[4] = {0, 0, 0, 0};
          char b2[4] = {' ', ' ', ' ', 0};
          itoa(state.buttons_8, b, 10);
          if (state.buttons_8 >= 100)
            std::copy(b, b + 3, b2 + 1);
          else if (state.buttons_8 >= 10)
            std::copy(b, b + 2, b2 + 2);
          else
            std::copy(b, b + 2, b2 + 3);
          draw_string(dot_matrix, b2, false);
        } else {
          state.scroll_dotmatrix = false;
          if (state.buttons_8 == 1) {
            draw_string(dot_matrix, "MAMA", false);
          }
          if (state.buttons_8 == 2) {
            draw_string(dot_matrix, "PAPA", false);
          }
          if (state.buttons_8 == 4) {
            state.scroll_dotmatrix = true;
            show_text_and_scroll(dot_matrix, "JANNIS    ");
          }
          if (state.buttons_8 == 8) {
            draw_string(dot_matrix, "MARA", false);
          }
          if (state.buttons_8 == 16) {
            draw_string(dot_matrix, "LUAN", false);
          }
        }

        pico7219_flush(dot_matrix);

        arcade8_num_changed = false;
      }
      auto end = time_us_32();

      switch6_changed = false;

      if (state.tick % FPS == 0) {
        std::cout << (int)state.switch6 << std::endl;
      }
#if 0
      // debug frames per second
      auto dur = end - start;
      frame_usec_max = std::max(frame_usec_max, dur);
      frame_usec_min = std::min(frame_usec_min, dur);
      if (state.tick % FPS == 0) {
        std::cout << "frame time min=" << frame_usec_min
                  << ", max=" << frame_usec_max << " usec" << std::endl;
      }
      if (state.tick % FPS == 0) {
        for (int k = 0; k < 4; ++k) {
          // adc_value_f = ads1115_raw_to_volts(adc_value, &adc);
          std::cout << "ADC: " << (int)state.faders[k] << " ";

        }
        std::cout << std::endl;

        std::cout << "phone dialing: " << phone_pulse.rising_edge_count_ << ", "
          << phone_pulse.falling_edge_count_ << std::endl;
        std::cout << "phone dialing_in_progress: " << phone_dialing_in_progress.rising_edge_count_ << ", "
          << phone_dialing_in_progress.falling_edge_count_ << std::endl;
        std::cout << "phone pulse: " << phone_pulse.rising_edge_count_ << ", "
          << phone_pulse.falling_edge_count_ << std::endl;
        std::cout << "phone dialed nunm " << (int)state.phone_dialed_num << std::endl;
      }
#endif
    }
  }

  return 0;
}
