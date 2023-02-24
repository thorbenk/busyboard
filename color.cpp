#include "color.h"

#include <algorithm>
#include <cmath>

auto hsv_to_rgb(float H, float S, float V) -> std::tuple<float, float, float> {
  float h = std::floor(H / 60.f);
  float f = H / 60.f - h;
  float p = V * (1 - S);
  float q = V * (1 - S * f);
  float t = V * (1 - S * (1 - f));

  if (h == 0 || h == 6)
    return std::make_tuple(V, t, p);
  else if (h == 1)
    return std::make_tuple(q, V, p);
  else if (h == 2)
    return std::make_tuple(p, V, t);
  else if (h == 3)
    return std::make_tuple(p, q, V);
  else if (h == 4)
    return std::make_tuple(t, p, V);
  else if (h == 5)
    return std::make_tuple(V, p, q);
  return std::make_tuple(0, 0, 0);
}
