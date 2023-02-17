#include "debounce.h"

int64_t debounce_alarm(alarm_id_t id, void *user_data) {
  Debounce *d = reinterpret_cast<Debounce *>(user_data);
  d->alarm_running_ = false;
  d->state_ = gpio_get(d->pin_);
  return 0;
}
