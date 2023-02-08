#include "pico/stdlib.h"
#include <stdio.h>

#include <PicoLed.hpp>

#define LED_PIN 13
#define LED_LENGTH 1

int main() {
  stdio_init_all();

  auto ledStrip = PicoLed::addLeds<PicoLed::WS2812B>(
      pio0, 0, LED_PIN, LED_LENGTH, PicoLed::FORMAT_GRB);
  ledStrip.setBrightness(128);
  ledStrip.clear();

  ledStrip.fill(PicoLed::RGB(255, 0, 0));
  ledStrip.show();

  while (true) {
    ledStrip.fill(PicoLed::RGB(255, 0, 0));
    ledStrip.show();
    sleep_ms(2000);
    ledStrip.fill(PicoLed::RGB(0, 255, 0));
    ledStrip.show();
    sleep_ms(2000);
    ledStrip.fill(PicoLed::RGB(0, 0, 255));
    ledStrip.show();
    sleep_ms(2000);
  }

  return 0;
}
