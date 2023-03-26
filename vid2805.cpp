#include "vid2805.h"

uint8_t detail::microsteps_sine_[24] = {114, 102, 88,  72,  55,  39,  25,  13,
                                        5,   1,   1,   5,   13,  25,  39,  55,
                                        72,  88,  102, 114, 122, 126, 126, 122};

uint16_t detail::accel_table_[32];
