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

#include "arcade_sounds.h"
#include "color.h"
#include "debounce.h"
#include "dfPlayerDriver.h"
#include "dotmatrix.h"
#include "fan_leds.h"
#include "modes.h"
#include "phone.h"
#include "sound_game.h"

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
// GP  7
// GP  8 - UART1 TX -> DF Player Mini
// GP  9 - UART1 TX -> DF Player Mini
// GP 10 - SPI1 SCK -> Dot matrix CLOCK
// GP 11 - SPI1 TX  -> Dot matrix DIN
// GP 12 - <reserved for SPI1 RX?>
// GP 13 - SPI1 CSn -> Dot matrix CS
// GP 14
// GP 15
//
// GP 16 - I2C0 SDA
// GP 17 - I2C1 SDL
// GP 18 - phone dial: pulsed number (brown cable)
// GP 19
// GP 20 - data in for WS2812b: string of 8 arcade buttons and 6 LEDs in fan
// GP 21 - fan PWM signal
// GP 22 - phone dial: dial in progress (green cable)
// GP 23
// GP 24
// GP 25
// GP 26 - data in for WS2812b: string of 9 WRGB for phone dial
// GP 27 - IO expander 2 interrupt
// GP 28 - IO expander 1 interrupt
//----------------------------------------------------------------------------

#define I2C_0_SDA_PIN 16
#define I2C_0_SDL_PIN 17
#define I2C_0_BAUD_RATE 100 * 1000

#define I2C_1_SDA_PIN 2
#define I2C_1_SDL_PIN 3
#define I2C_1_BAUD_RATE 100 * 1000

#define IO_EXPAND_16_DEVICE_1_I2C_LANE i2c1
#define IO_EXPAND_16_DEVICE_1_INTERRUPT_PIN 28
#define IO_EXPAND_16_DEVICE_1_I2C_ADDRESS 0x20
#define IO_EXPAND_16_DEVICE_1_DEBOUNCE_MSEC 2

#define IO_EXPAND_16_DEVICE_2_I2C_LANE i2c0
#define IO_EXPAND_16_DEVICE_2_INTERRUPT_PIN 27
#define IO_EXPAND_16_DEVICE_2_I2C_ADDRESS 0x21
#define IO_EXPAND_16_DEVICE_2_DEBOUNCE_MSEC 2

#define ARCADE_BUTTONS_8_DIN_PIN 20
#define ARCADE_BUTTONS_8_LED_LENGTH 8
#define ARCADE_BUTTONS_1_LED_LENGTH 1
#define FAN_LED_LENGTH 6
#define FAN_PWM_PIN 21
constexpr auto arcade_buttons_8_led_format = PicoLed::FORMAT_GRB;
constexpr auto arcade_buttons_1_led_format = PicoLed::FORMAT_GRB;
constexpr auto fan_led_format = PicoLed::FORMAT_GRB;
static_assert(arcade_buttons_8_led_format == fan_led_format);
static_assert(arcade_buttons_8_led_format == arcade_buttons_1_led_format);
constexpr auto grb_led_string_length =
    ARCADE_BUTTONS_8_LED_LENGTH + ARCADE_BUTTONS_1_LED_LENGTH + FAN_LED_LENGTH;

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
#define ARCADE_1_COLOR_RETAIN_TIME_MS 9000

constexpr uint16_t fader_min_max[4][2]{
    {72, 19813}, {64, 19707}, {121, 19657}, {84, 19787}};

//----------------------------------------------------------------------------
// globals
//----------------------------------------------------------------------------

DfPlayerPico<DFPLAYER_UART, DFPLAYER_MINI_TX, DFPLAYER_MINI_RX> *dfp = nullptr;

Debounce_PCF8575 io16_dev1(IO_EXPAND_16_DEVICE_1_I2C_LANE,
                           IO_EXPAND_16_DEVICE_1_I2C_ADDRESS,
                           IO_EXPAND_16_DEVICE_1_DEBOUNCE_MSEC);

Debounce_PCF8575 io16_dev2(IO_EXPAND_16_DEVICE_2_I2C_LANE,
                           IO_EXPAND_16_DEVICE_2_I2C_ADDRESS,
                           IO_EXPAND_16_DEVICE_2_DEBOUNCE_MSEC);

// DebounceEdge phone_pulse(5);
DebounceEdge phone_dialing_in_progress(50);

struct ads1115_adc adc;

volatile bool io16_device1_changed = true;
std::optional<uint16_t> io16_device1_prev_state;
std::optional<uint16_t> io16_device2_prev_state;

struct State {
  uint8_t buttons_8 = 0;
  uint32_t tick = 0;
  FaderMode fader_mode = FaderMode::RGB;
  ArcadeMode arcade_mode = ArcadeMode::Binary;
  bool toggle_upper_left = true;
  bool double_switch[2] = {false, false};
  bool double_toggle[2] = {false, false};
  bool arcade_1_pressed = false;
  uint32_t arcade_1_pressed_since_ms = 0;
  uint8_t faders[4] = {0, 0, 0, 0};
  uint8_t fader_adc = 0;
  bool dial_in_progress = false;
  int8_t phone_dialed_num = -1;
  int8_t switch6 = 0;
  bool scroll_dotmatrix = false;
};

struct Leds {
  PicoLed::Color grb_led_string[grb_led_string_length];
  PicoLed::Color fader_analog_string[fader_and_analog_meter_led_string_length];
  PicoLed::Color phone_leds[PHONE_LEDS_LENGTH];
};

State state;
std::optional<State> prev_state;
Leds leds;
volatile bool frame_changed = true;

SoundGame sound_game;
Phone phone;
FanLEDs fan_leds;

//----------------------------------------------------------------------------

void gpio_interrupt(uint gpio, uint32_t events) {
  if (gpio == PHONE_DIAL_PULSED_NUMBER) {
    // phone_pulse.on_event(events);
  } else if (gpio == PHONE_DIAL_IN_PROGRESS_PIN) {
    phone_dialing_in_progress.on_event(events);
  } else if (gpio == IO_EXPAND_16_DEVICE_1_INTERRUPT_PIN) {
    io16_dev1.on_pcf8575_interrupt();
  } else if (gpio == IO_EXPAND_16_DEVICE_2_INTERRUPT_PIN) {
    io16_dev2.on_pcf8575_interrupt();
  }
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

  if (!prev_state.has_value() || prev_state->fader_mode != state.fader_mode) {
    fan_leds.set_mode(state.fader_mode);
  }

  state.phone_dialed_num = phone.dialed_number();

  auto f = state.tick % duration_frames;
  if (f > duration_frames_2)
    f = duration_frames - f;
  uint8_t v = 32 + 128 * (f / static_cast<float>(duration_frames_2));

  float hue_on = 120.0;
  float hue_off = 0.0;

  //
  // fader panel LEDs
  //

  if (state.double_switch[0]) {
    if (state.fader_mode == FaderMode::RGB) {
      hue_on = 120.0;
      hue_off = 0.0;
      leds.fader_analog_string[0] = PicoLed::RGB(128, 0, 0);
      leds.fader_analog_string[1] = PicoLed::RGB(0, 0, 0);
      leds.fader_analog_string[2] = PicoLed::RGB(0, 0, 0);
    } else if (state.fader_mode == FaderMode::HSV) {
      hue_on = 60.0;
      hue_off = 180.0;
      leds.fader_analog_string[0] = PicoLed::RGB(0, 0, 0);
      leds.fader_analog_string[1] = PicoLed::RGB(0, 128, 0);
      leds.fader_analog_string[2] = PicoLed::RGB(0, 0, 0);
    } else if (state.fader_mode == FaderMode::Effect) {
      hue_on = 200.0;
      hue_off = 300.0;
      leds.fader_analog_string[0] = PicoLed::RGB(0, 0, 0);
      leds.fader_analog_string[1] = PicoLed::RGB(0, 0, 0);
      leds.fader_analog_string[2] = PicoLed::RGB(0, 0, 128);
    }
  } else {
    leds.fader_analog_string[0] = PicoLed::RGB(0, 0, 0);
    leds.fader_analog_string[1] = PicoLed::RGB(0, 0, 0);
    leds.fader_analog_string[2] = PicoLed::RGB(0, 0, 0);
  }

  // Fan speed
  // Lowest speed should be 10%
  // The PWM is inverted:
  uint8_t const fan_pwm = std::max(static_cast<uint8_t>(25), state.faders[0]);
  pwm_set_gpio_level(FAN_PWM_PIN, fan_pwm);

  //
  // 8 arcade buttons RGB lights
  //
  if (state.arcade_mode == ArcadeMode::Names ||
      state.arcade_mode == ArcadeMode::Binary) {
    for (int i = 0; i < ARCADE_BUTTONS_8_LED_LENGTH; ++i) {
      float v_arcade = v;
      if (!state.toggle_upper_left)
        v_arcade = 0.f;

      if ((1 << i) & state.buttons_8) {
        // button pressed
        auto const [r, g, b] = hsv_to_rgb(hue_on, 1.0, v_arcade);
        leds.grb_led_string[i] = PicoLed::RGB(r, g, b);
      } else {
        // button not pressed
        auto const [r, g, b] = hsv_to_rgb(hue_off, 1.0, v_arcade);
        leds.grb_led_string[i] = PicoLed::RGB(r, g, b);
      }
    }
  } else if (state.arcade_mode == ArcadeMode::SoundGame) {
    if (!prev_state.has_value() ||
        state.toggle_upper_left != prev_state->toggle_upper_left) {
      sound_game.set_enabled(state.toggle_upper_left);
    }
    sound_game.calc_frame(&leds.grb_led_string[0]);
  } else {
    // unimplemented
  }

  //
  // 1 arcade buttons RGB light
  // (phone arcade button)
  //

  PicoLed::Color arcade1_color;
  if (state.arcade_1_pressed_since_ms > 0) {
    auto elapsed =
        to_ms_since_boot(get_absolute_time()) - state.arcade_1_pressed_since_ms;
    if (elapsed <= ARCADE_1_COLOR_RETAIN_TIME_MS) {
      arcade1_color = PicoLed::RGB(0, 0, 128);
    } else {
      state.arcade_1_pressed_since_ms = 0;
    }
  } else {
    arcade1_color = PicoLed::RGB(0, 128, 0);
  }

  if (state.double_toggle[0]) {
    if (state.dial_in_progress) {
      leds.grb_led_string[ARCADE_BUTTONS_8_LED_LENGTH] =
          PicoLed::RGB(128, 128, 0); // yellow
    } else {
      leds.grb_led_string[ARCADE_BUTTONS_8_LED_LENGTH] = arcade1_color;
    }
  } else {
    leds.grb_led_string[ARCADE_BUTTONS_8_LED_LENGTH] =
        PicoLed::RGB(0, 0, 0); // off
  }

  //
  // analog meter RGB lights
  //
  for (int i = FADER_LED_LENGTH; i < fader_and_analog_meter_led_string_length;
       ++i) {
    if (state.double_switch[1]) {
      leds.fader_analog_string[i] = PicoLed::RGBW(0, 0, 0, 64);
    } else {
      leds.fader_analog_string[i] = PicoLed::RGB(0, 0, 0);
    }
  }

  if (!prev_state.has_value() ||
      prev_state->double_toggle[0] != state.double_toggle[0]) {
    phone.switch_on(state.double_toggle[0]);
  }
  phone.calc_frame(leds.phone_leds);

  //
  // fan RGB lights
  //
  if (!prev_state.has_value() ||
      prev_state->double_switch[0] != state.double_switch[1]) {
    fan_leds.set_enabled(state.double_switch[0]);
  }
  fan_leds.calc_frame(leds.grb_led_string + ARCADE_BUTTONS_8_LED_LENGTH +
                          ARCADE_BUTTONS_1_LED_LENGTH,
                      FAN_LED_LENGTH, state.faders);
}

int64_t on_frame(alarm_id_t id, void *user_data) {
  ++state.tick;
  frame_changed = true;
  return MS_PER_FRAME * 1000; // microseconds
}

//---------------------------------------------------------------------------

void play_sound(ArcadeSounds sound) {
  uint8_t *bytes = reinterpret_cast<uint8_t *>(&sound);
  uint8_t folder = bytes[1];
  uint8_t track = bytes[0];
  uint16_t cmd = (folder << 8) | track;
  dfp->sendCmd(dfPlayer::SPECIFY_FOLDER_PLAYBACK, cmd);
}

void play_sound(uint8_t folder, uint8_t track) {
  uint16_t cmd = (folder << 8) | track;
  dfp->sendCmd(dfPlayer::SPECIFY_FOLDER_PLAYBACK, cmd);
}

void display_number(Pico7219 *dot_matrix, uint8_t number) {
  char b[4] = {0, 0, 0, 0};
  char b2[4] = {' ', ' ', ' ', 0};
  itoa(number, b, 10);
  if (number >= 100)
    std::copy(b, b + 3, b2 + 1);
  else if (state.buttons_8 >= 10)
    std::copy(b, b + 2, b2 + 2);
  else
    std::copy(b, b + 2, b2 + 3);
  draw_string(dot_matrix, b2, false);
}

//----------------------------------------------------------------------------

int main() {
  stdio_init_all();

  i2c_init(i2c0, I2C_0_BAUD_RATE);
  gpio_set_function(I2C_0_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C_0_SDL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_0_SDA_PIN);
  gpio_pull_up(I2C_0_SDL_PIN);

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

  gpio_init(IO_EXPAND_16_DEVICE_2_INTERRUPT_PIN);
  gpio_set_dir(IO_EXPAND_16_DEVICE_2_INTERRUPT_PIN, GPIO_IN);
  gpio_pull_up(IO_EXPAND_16_DEVICE_2_INTERRUPT_PIN);
  gpio_set_irq_enabled_with_callback(IO_EXPAND_16_DEVICE_2_INTERRUPT_PIN,
                                     GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
                                     true, &gpio_interrupt);

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

  dfp = new DfPlayerPico<DFPLAYER_UART, DFPLAYER_MINI_TX, DFPLAYER_MINI_RX>();
  dfp->reset();
  sleep_ms(2000);
  dfp->specifyVolume(15);
  sleep_ms(200);

  io16_dev1.init();
  io16_dev2.init();

  bool arcade8_num_changed = false;

  add_alarm_in_ms(MS_PER_FRAME, on_frame, nullptr, false);

  int phone_dialing_prev = 0;

  bool last_num_switch = false;
  int num = -1;
  int display_num = -1;

  bool switch6_changed = false;
  bool toggle_upper_left_changed = false;

  while (true) {
    {
      state.dial_in_progress = !gpio_get(PHONE_DIAL_IN_PROGRESS_PIN);
      bool const num_switched = gpio_get(PHONE_DIAL_PULSED_NUMBER);

      phone.loop(state.dial_in_progress, num_switched);
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
          } else if (state.arcade_mode == ArcadeMode::Names) {
            state.buttons_8 = (1 << i);
          } else if (state.arcade_mode == ArcadeMode::SoundGame) {
            state.buttons_8 = (1 << i);
          }
        }
        if (i == 8 && prev == 1 && current == 0) {
          state.fader_mode = FaderMode::RGB;
        } else if (i == 9 && prev == 1 && current == 0) {
          state.fader_mode = FaderMode::HSV;
        } else if (i == 10 && prev == 1 && current == 0) {
          state.fader_mode = FaderMode::Effect;
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
        if (switch6_changed) {
          state.buttons_8 = 0;
          state.scroll_dotmatrix = false;
        }
      } else if (state.switch6 == 1) {
        state.arcade_mode = ArcadeMode::Names;
        if (switch6_changed) {
          state.buttons_8 = 1 << 4;
          state.scroll_dotmatrix = false;
        }
      } else {
        state.arcade_mode = ArcadeMode::SoundGame;
        if (switch6_changed) {
          state.buttons_8 = 0;
          state.scroll_dotmatrix = false;
        }
      }
      io16_device1_prev_state = io16_dev1.state();
    }
    if (io16_dev2.loop()) {
      auto const current_state = io16_dev2.state();
      std::cout << "io16 dev 2 changed to " << std::bitset<16>(current_state)
                << std::endl;
      for (auto i = 0; i < 16; ++i) {
        bool const current = ((1 << i) & current_state) > 0;
        bool const prev = io16_device2_prev_state.has_value()
                              ? (((1 << i) & *io16_device2_prev_state) > 0)
                              : (!current);

        if (i == 0) {
          state.toggle_upper_left = (current == 1);
          if (current != prev) {
            toggle_upper_left_changed = true;
          }
        } else if (i == 1 && prev == 1 && current == 0) {
          state.double_toggle[1] = !state.double_toggle[1];
        } else if (i == 2 && prev == 1 && current == 0) {
          state.double_toggle[0] = !state.double_toggle[0];
        } else if (i == 3) {
          state.double_switch[0] = (current == 0);
        } else if (i == 4) {
          state.double_switch[1] = (current == 0);
        } else if (i == 5 && prev == 1 && current == 0) {
          state.arcade_1_pressed = true;
          state.arcade_1_pressed_since_ms =
              to_ms_since_boot(get_absolute_time());
        }
      }
    }

    if (frame_changed) {
#ifdef DEBUG_TIMING
      auto start = time_us_32();
#endif

      read_adc();

      calc_frame();
      frame_changed = false;

      if (state.scroll_dotmatrix && state.tick % 5 == 0) {
        pico7219_scroll(dot_matrix, true);
      }

      if (prev_state.has_value() &&
          prev_state->phone_dialed_num != state.phone_dialed_num) {
        play_sound(1, state.phone_dialed_num);
      }

      for (int i = 0; i < grb_led_string_length; ++i) {
        arcade_and_fan_leds.setPixelColor(i, leds.grb_led_string[i]);
      }
      arcade_and_fan_leds.show();

      for (int i = 0; i < fader_and_analog_meter_led_string_length; ++i) {
        fader_and_analog_meter_leds.setPixelColor(i,
                                                  leds.fader_analog_string[i]);
      }
      fader_and_analog_meter_leds.show();

      for (int i = 0; i < PHONE_LEDS_LENGTH; ++i) {
        phone_leds.setPixelColor(i, leds.phone_leds[i]);
      }
      phone_leds.show();

      if (arcade8_num_changed || switch6_changed || toggle_upper_left_changed) {
        std::cout << "update dot matrix" << std::endl;
        pico7219_switch_off_all(dot_matrix, false);
        if (state.toggle_upper_left) {
          if (state.arcade_mode == ArcadeMode::Binary) {
            display_number(dot_matrix, state.buttons_8);
          } else if (state.arcade_mode == ArcadeMode::Names) {
            state.scroll_dotmatrix = false;
            if (state.buttons_8 == 1) {
              draw_string(dot_matrix, "MAMA", false);
              play_sound((state.tick % 3) + 2, 1);
            }
            if (state.buttons_8 == 2) {
              draw_string(dot_matrix, "PAPA", false);
              play_sound((state.tick % 3) + 2, 2);
            }
            if (state.buttons_8 == 4) {
              state.scroll_dotmatrix = true;
              show_text_and_scroll(dot_matrix, "JANNIS    ");
              play_sound((state.tick % 3) + 2, 3);
            }
            if (state.buttons_8 == 8) {
              draw_string(dot_matrix, "MARA", false);
              play_sound((state.tick % 3) + 2, 4);
            }
            if (state.buttons_8 == 16) {
              draw_string(dot_matrix, "LUAN", false);
              play_sound((state.tick % 3) + 2, 5);
            }
          } else if (state.arcade_mode == ArcadeMode::SoundGame) {
            state.scroll_dotmatrix = false;
            // if (sound_game.should_play_sound()) {
            if (true) {

              uint8_t button = 0;
              for (int i = 0; i < 8; ++i) {
                if (((1 << i) & state.buttons_8) > 0) {
                  button = i;
                }
              }

              if (prev_state.has_value() &&
                  state.buttons_8 != prev_state->buttons_8) {
                std::cout << "ARCADE BUTTON " << (int)button << std::endl;
                auto sound = sound_game.sound_for_button(button);
                play_sound(sound);
                state.buttons_8 = 0;
              }
            }
          }
        }
        pico7219_flush(dot_matrix);
      }
      arcade8_num_changed = false;
      switch6_changed = false;
      toggle_upper_left_changed = false;

      if (state.arcade_1_pressed) {
        std::cout << "ARCADE 1 PRESSED" << std::endl;
        play_sound(1, 10);
      }

      state.arcade_1_pressed = false;

#ifdef DEBUG_TIMING
      auto end = time_us_32();

      // debug frames per second
      auto dur = end - start;
      frame_usec_max = std::max(frame_usec_max, dur);
      frame_usec_min = std::min(frame_usec_min, dur);
      if (state.tick % FPS == 0) {
        std::cout << "FRAME time min=" << frame_usec_min
                  << ", max=" << frame_usec_max << " usec" << std::endl;
      }
      if (state.tick % FPS == 0) {
        for (int k = 0; k < 4; ++k) {
          // adc_value_f = ads1115_raw_to_volts(adc_value, &adc);
          std::cout << "ADC: " << (int)state.faders[k] << " ";
        }
        std::cout << std::endl;
      }
#endif

      prev_state = state;
    }
  }

  return 0;
}
