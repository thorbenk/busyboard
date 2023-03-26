#pragma once

#include <pico7219/pico7219.h>

void draw_string(Pico7219 *pico7219, const char *s, bool flush);
void show_text_and_scroll(Pico7219 *pico7219, const char *string);
