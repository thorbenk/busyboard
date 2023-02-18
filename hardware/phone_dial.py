#!/usr/bin/env python3

import svgwrite
from svgwrite import mm
import math
import numpy as np


def mm_tup(args):
    t = tuple(["{}mm".format(a) for a in args])
    return t


# ---
led_count = 9
screw_hole_diam = 71
case_inner_diam = 77
case_outer_diam = 82
dist_to_led_ring = 15
led_diam = 8
outer_margin = 10
# ---

A4 = np.asarray((210.0, 297.0))
A4_center = A4 / 2.0

A4c_mm = (mm(A4_center[0]), mm(A4_center[1]))


dwg = svgwrite.Drawing(
    "phone_dial.svg", debug=True, profile="full", size=(mm(A4[0]), mm(A4[1]))
)


black = svgwrite.rgb(0, 0, 0)

dwg.add(
    dwg.circle(
        A4c_mm,
        mm(screw_hole_diam / 2.0),
        stroke=svgwrite.rgb(128, 128, 128),
        fill="none",
        stroke_width=mm(0.5),
    )
)
dwg.add(
    dwg.circle(
        A4c_mm,
        mm(case_inner_diam / 2.0),
        stroke=black,
        fill="none",
        stroke_width=mm(0.5),
    )
)
dwg.add(
    dwg.circle(
        A4c_mm,
        mm(case_outer_diam / 2.0),
        stroke=black,
        fill="none",
        stroke_width=mm(0.5),
    )
)
dwg.add(
    dwg.circle(
        A4c_mm,
        mm(case_outer_diam / 2.0 + dist_to_led_ring),
        stroke=black,
        fill="none",
        stroke_width=mm(0.5),
    )
)
dwg.add(
    dwg.circle(
        A4c_mm,
        mm(case_outer_diam / 2.0 + dist_to_led_ring + led_diam),
        stroke=black,
        fill="none",
        stroke_width=mm(0.5),
    )
)
outer_r = case_outer_diam / 2.0 + dist_to_led_ring + led_diam + outer_margin

dwg.add(
    dwg.circle(A4c_mm, mm(outer_r), stroke=black, fill="none", stroke_width=mm(0.5))
)

dwg.add(
    dwg.line(
        start=mm_tup(A4_center - np.asarray([outer_r, 0])),
        end=mm_tup(A4_center + np.asarray([outer_r, 0])),
        stroke_width=mm(0.5),
        stroke=svgwrite.rgb(255, 0, 0),
    )
)

screw_support_x = 10
screw_support_y = 10

dwg.add(
    dwg.rect(
        (
            mm(A4_center[0] + case_inner_diam / 2.0 - screw_support_x),
            mm(A4_center[1] - screw_support_y / 2.0),
        ),
        (mm(10), mm(10)),
        fill="none",
        stroke=black,
    )
)
dwg.add(
    dwg.rect(
        (
            mm(A4_center[0] - case_inner_diam / 2.0),
            mm(A4_center[1] - screw_support_y / 2.0),
        ),
        (mm(10), mm(10)),
        fill="none",
        stroke=black,
    )
)

initial_alpha = 0
for i in range(led_count):
    step = 360.0 / led_count
    alpha = initial_alpha + i * step

    print(alpha)

    r = 82 / 2.0 + 15 + 8 / 2

    x = r * math.sin(math.radians(alpha))
    y = r * math.cos(math.radians(alpha))

    c = (A4_center[0] + x, A4_center[1] + y)

    dwg.add(
        dwg.circle(
            (mm(c[0]), mm(c[1])),
            led_diam / 2.0 * mm,
            stroke=svgwrite.rgb(0, 0, 0),
            fill="none",
            stroke_width=mm(0.5),
        )
    )
    dwg.add(dwg.text("{}".format(i), insert=(mm(c[0]), mm(c[1]))))


dwg.save(pretty=True)
