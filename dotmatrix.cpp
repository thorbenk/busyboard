#include "dotmatrix.h"

#include <cstring>

extern uint8_t console_font_8x8[];

// Draw a character to the library. Note that the width of the "virtual
// display" can be much longer than the physical module chain, and
// off-display elements can later be scrolled into view. However, it's
// the job of the application, not the library, to size the virtual
// display sufficiently to fit all the text in.
//
// chr is an offset in the font table, which starts with character 32 (space).
// It isn't an ASCII character.
void draw_character(Pico7219 *pico7219, uint8_t chr, int x_offset, BOOL flush) {
  for (int i = 0; i < 8; i++) // row
  {
    // The font elements are one byte wide even though, as its an 8x5 font,
    //   only the top five bits of each byte are used.
    uint8_t v = console_font_8x8[8 * chr + i];
    for (int j = 0; j < 8; j++) // column
    {
      int sel = 1 << j;
      if (sel & v)
        pico7219_switch_on(pico7219, 7 - i, 7 - j + x_offset, FALSE);
    }
  }
  if (flush)
    pico7219_flush(pico7219);
}

// Draw a string of text on the (virtual) display. This function assumes
// that the library has already been configured to provide a virtual
// chain of LED modules that is long enough to fit all the text onto.
void draw_string(Pico7219 *pico7219, const char *s, bool flush) {
  int x = 0;
  while (*s) {
    draw_character(pico7219, *s, x, FALSE);
    s++;
    x += 8;
  }
  if (flush)
    pico7219_flush(pico7219);
}

// Get the number of horizontal pixels that a string will take. Since each
// font element is five pixels wide, and there is one pixel between each
// character, we just multiply the string length by 6.
int get_string_length_pixels(const char *s) { return std::strlen(s) * 8; }

// Get the number of 8x8 LED modules that would be needed to accomodate the
// string of text. That's the number of pixels divided by 8 (the module
// width), and then one added to round up.
int get_string_length_modules(const char *s) {
  return get_string_length_pixels(s) / 8 + 1;
}

// Show a string of characters, and then scroll it across the display.
// This function uses pico7219_set_virtual_chain_length() to ensure that
// there are enough "virtual" modules in the display chain to fit
// the whole string. It then scrolls it enough times to scroll the
// whole string right off the end.
void show_text_and_scroll(Pico7219 *pico7219, const char *string) {
  pico7219_set_virtual_chain_length(pico7219,
                                    get_string_length_modules(string));
  draw_string(pico7219, string, false);
  pico7219_flush(pico7219);

  int l = get_string_length_pixels(string);
}
