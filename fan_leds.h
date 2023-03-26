#include <PicoLed.hpp>
#include <array>

#include "modes.h"

class FanLEDs {
public:
  FanLEDs() = default;

  void set_enabled(bool);
  void set_mode(FaderMode);

  void calc_frame(PicoLed::Color *strip_begin, uint8_t led_count,
                  uint8_t *faders);

private:
  void next_frame();
  bool enabled_ = false;
  uint32_t frame_ = 0;
  FaderMode mode_ = FaderMode::RGB;
};
