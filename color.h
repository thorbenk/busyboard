#pragma once

#include <tuple>

// H in [0, 360)
// S in [0.0, 1.0]
// V in [0, 256)
auto hsv_to_rgb(float H, float S, float V) -> std::tuple<float, float, float>;
