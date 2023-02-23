#include "hardware/gpio.h"
#include "pico/stdlib.h"
extern "C" {
#include <PicoTM1637.h>
}

#define DIGITS_CLK_PIN 21
#define DIGITS_DIO_PIN 20

int main() {
  stdio_init_all();
  TM1637_init(DIGITS_CLK_PIN, DIGITS_DIO_PIN);
  TM1637_clear();
  TM1637_set_brightness(3);

  int num = 0;
  while (true) {
    TM1637_display(num, false);
    num += 1;
    if (num > 9999)
      num = 0;
    sleep_ms(1000);
  }

  return 0;
}
