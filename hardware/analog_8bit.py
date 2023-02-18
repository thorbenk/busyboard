#!/usr/bin/env python3

import svgwrite
from svgwrite import mm
import math
import numpy as np

black = svgwrite.rgb(0, 0, 0)


def add_cross(dwg, pos, size):
    s = size / 2.0
    l1 = dwg.line(
        (pos[0] - s, pos[1] - s),
        (pos[0] + s, pos[1] + s),
        stroke=black,
        stroke_width=stroke_width,
    )
    l2 = dwg.line(
        (pos[0] - s, pos[1] + s),
        (pos[0] + s, pos[1] - s),
        stroke=black,
        stroke_width=stroke_width,
    )
    dwg.add(l1)
    dwg.add(l2)


def add_line(dwg, s, e, thickness):
    l = dwg.line((s[0], s[1]), (e[0], e[1]), stroke=black, stroke_width=thickness)
    dwg.add(l)


def add_text(dwg, text, center, rotation, font_size):
    t = dwg.text(
        text,
        insert=(center[0], center[1]),
        text_anchor="middle",
        alignment_baseline="middle",
        transform="rotate({}, {}, {})".format(rotation, center[0], center[1]),
        style="font-size:{}; font-family:Roboto".format(font_size),
    )
    dwg.add(t)


# ---
N = 256
R = 50
major_ticks = 32
minor_ticks = 8
major_tick_length = 10
minor_tick_length = 7
major_tick_thickness = 3
minor_tick_thickness = 1
label_offset = 2
font_size = 8
window_padding = 20
window_rounding = 10
margin_beyond_window = 15
margin_beyond_window_extra_for_lighting = 15
lcd_width = 30
lcd_height = 14
lcd_y_offset = 15
stroke_width = 0.5
# ---

A4 = np.asarray((297.0, 210.0))
drawing_center = A4 / 2.0 + np.asarray((0, 30))


dwg = svgwrite.Drawing(
    "analog_8bit.svg",
    debug=True,
    profile="full",
    size=(mm(A4[0]), mm(A4[1])),
    viewBox="{} {} {} {}".format(0, 0, A4[0], A4[1]),
)
# one "user unit" is now on mm

add_cross(dwg, drawing_center, 5)

xs = []
ys = []
alpha = np.asarray((-(90.0 + 45 / 2.0), (90 + 45 / 2.0)))
for i, a in enumerate(np.linspace(alpha[0], alpha[1], N + 1)):
    n = np.asarray((math.sin(math.radians(a)), -math.cos(math.radians(a))))
    s = drawing_center
    if i % major_ticks == 0:
        p1 = s + R * n
        p2 = s + (R + major_tick_length) * n
        p3 = s + (R + major_tick_length + label_offset) * n
        add_line(dwg, p1, p2, major_tick_thickness)
        add_text(dwg, "{}".format(i), p3, a, font_size)
        xs.append(p3[0])
        ys.append(p3[1])

    elif i % minor_ticks == 0:
        add_line(dwg, s + R * n, s + (R + minor_tick_length) * n, minor_tick_thickness)

min_x, max_x = np.min(xs), np.max(xs)
min_y, max_y = np.min(ys), np.max(ys)

# Add rounded rect for the viewing window
dwg.add(
    dwg.rect(
        (min_x - window_padding, min_y - window_padding),
        (max_x - min_x + 2 * window_padding, max_y - min_y + 2 * window_padding),
        fill="none",
        rx=window_rounding,
        ry=window_rounding,
        stroke=black,
        stroke_width=stroke_width,
    )
)

# Add rect for the border around the viewing window
dwg.add(
    dwg.rect(
        (
            min_x - window_padding - margin_beyond_window,
            min_y
            - window_padding
            - margin_beyond_window
            - margin_beyond_window_extra_for_lighting,
        ),
        (
            max_x - min_x + 2 * window_padding + 2 * margin_beyond_window,
            max_y
            - min_y
            + 2 * window_padding
            + 2 * margin_beyond_window
            + margin_beyond_window_extra_for_lighting,
        ),
        fill="none",
        stroke=black,
        stroke_width=stroke_width,
    )
)

dwg.add(
    dwg.rect(
        (
            min_x - window_padding + window_rounding,
            min_y - window_padding - margin_beyond_window_extra_for_lighting,
        ),
        (
            max_x - min_x + 2 * window_padding - 2 * window_rounding,
            margin_beyond_window_extra_for_lighting,
        ),
        fill="none",
        stroke=black,
        stroke_width=stroke_width,
    )
)

# Add 4-digit 7-segment LCD cutout
dwg.add(
    dwg.rect(
        drawing_center + np.asarray((-lcd_width / 2, lcd_y_offset)),
        (lcd_width, lcd_height),
        fill="none",
        stroke=black,
        stroke_width=stroke_width,
    )
)

dwg.save(pretty=True)
