#pragma once

#include <tuple>

auto hsv_to_rgb(float H, float S, float V) -> std::tuple<float, float, float>;
