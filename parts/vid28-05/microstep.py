#!/usr/bin/env python3

import numpy as np
import math
import sys
from matplotlib import pyplot as plt
from functools import partial

def microstep_table_from_pdf_in_mV():
    # See VID Micro Step Calculation Table PDF
    PM = 15.30 # mA
    P1 = 14.80 # mA
    P2 = 13.25 # mA
    P3 = 9.75  # mA
    P4 = 5.59  # mA
    P5 = 2.85  # mA

    R_COIL = 280 # Ohm

    signal1 = np.asarray([-P4, -P5, 0, P5, P4, P3, P2, P1, PM, P1, P2, P3])
    signal1 = np.hstack([signal1, -signal1]) # len = 24 one wavelength
    # adjust with respect to partial step diagram in the Stepper Motor 
    # Specification datasheet, 
    assert len(signal1) == WAVELENGTH

    signal2 = np.asarray([P4, P3, P2, P1, PM, P1, P2, P3, P4, P5, 0, -P5])
    signal2 = np.hstack([signal2, -signal2]) # len = 24 one wavelength
    signal2 = np.roll(signal2, 0)
    assert len(signal2) == WAVELENGTH

    # Compute Voltage
    signal1 = R_COIL * signal1/1000.
    signal2 = R_COIL * signal2/1000.
    
    return (signal1, signal2)


def partial_step_signals_from_pdf_in_binary():
    pin1  = np.asarray([1,0,0,0,1,1])
    pin23 = np.asarray([0,0,1,1,1,0])
    pin4  = np.asarray([1,1,1,0,0,0])

    def pin_signal(pin_seq):
        assert len(pin_seq) == WAVELENGTH/4
        result = []
        for p in pin_seq:
            for i in range(MICROSTEPS_PER_PARTIAL_STEP):
                result.append(p)
        assert len(result) == MICROSTEPS_PER_PARTIAL_STEP * WAVELENGTH/4, "{} vs {}*{}".format(len(result), N_PERIODS, WAVELENGTH)
        return np.asarray(result)

    p1 = pin_signal(pin1)
    p23 = pin_signal(pin23)
    p4 = pin_signal(pin4)
    
    return (p1, p23, p4)

#-------------------------------------------------------------------------------

WAVELENGTH = 24
N_PERIODS = 4
MICROSTEPS_PER_PARTIAL_STEP = 4
amplitude_voltage = 5 # [V]

signal1, signal2 = microstep_table_from_pdf_in_mV()
signal1 = np.roll(signal1, -8)
s1 = np.tile(signal1, N_PERIODS)
s1 = np.hstack([s1, s1[0]])
s2 = np.tile(signal2, N_PERIODS)
s2 = np.hstack([s2, s2[0]])

p1, p23, p4 = partial_step_signals_from_pdf_in_binary()

p1 = np.tile(p1, N_PERIODS)
p1 = np.hstack([p1, p1[0]])
p23 = np.tile(p23, N_PERIODS)
p23 = np.hstack([p23, p23[0]])
p4 = np.tile(p4, N_PERIODS)
p4 = np.hstack([p4, p4[0]])

def cos(A, period, shift_x, shift_y, x):
    return A*math.cos((x+shift_x)*2*math.pi/period) + shift_y

xs = np.linspace(0,N_PERIODS*WAVELENGTH,1000)

f1 = partial(cos, amplitude_voltage/2., 24, 2, amplitude_voltage/2.)
f23 = partial(cos, amplitude_voltage/2., 24, 10, amplitude_voltage/2.)
f4 = partial(cos, amplitude_voltage/2., 24, 18, amplitude_voltage/2.)

f1_vals = np.asarray([f1(x) for x in xs])
f23_vals = np.asarray([f23(x) for x in xs])
f4_vals = np.asarray([f4(x) for x in xs])

def polish_plot(ax):
    for i in range(0, N_PERIODS*WAVELENGTH+1):
        if i % MICROSTEPS_PER_PARTIAL_STEP != 0:
            ax.axvline(i, color="gray", lw=1, alpha=0.5)
            continue
        lw = 3 if i % WAVELENGTH == 0 else 1
        ax.axvline(i, color="gray", lw=lw)
    plt.ylabel("[mV]")

ax1 = plt.subplot(511)
ax1.step(range(len(p1)), amplitude_voltage*p1, where="post", label="pin 1")

pts = []
mid_pts = np.arange(N_PERIODS*WAVELENGTH) + 0.5
for i in mid_pts:
    v = round(f1(i) / amplitude_voltage * 127)
    pts.append(v)
pts = np.asarray(pts)

ax1.scatter(mid_pts, pts/127.0*amplitude_voltage, marker='x', s=4)

print(", ".join(["{}".format(p) for p in pts[0:24]]))

ax1.plot(xs, f1_vals, label="f1")
polish_plot(ax1)
plt.tick_params('x', labelbottom=False)

ax2 = plt.subplot(512, sharex=ax1)
ax2.step(range(len(p23)), amplitude_voltage*p23, where="post", label="pin 23")
ax2.plot(xs, f23_vals, label="f2")
polish_plot(ax2)
plt.tick_params('x', labelbottom=False)

ax3 = plt.subplot(513, sharex=ax2)
ax3.step(range(len(p4)), amplitude_voltage*p4, where="post", label="pin 4")
ax3.plot(xs, f4_vals, label="f3")
polish_plot(ax3)
plt.tick_params('x', labelbottom=False)

ax4 = plt.subplot(514, sharex=ax3)
ax4.step(range(len(p1)), amplitude_voltage*(p1-p23), where="post", label="U_L = pin1 - pin23")
ax4.step([i for i in range(len(s1))], s1, where="mid", label="signal 1")
ax4.plot(xs, f1_vals-f23_vals, label="f1-f23")
polish_plot(ax4)
plt.tick_params('x', labelbottom=False)

ax5 = plt.subplot(515, sharex=ax4)
ax5.step(range(len(p1)), amplitude_voltage*(p4-p23), where="post", label="U_r = pin4 - pin23")
ax5.step([i for i in range(len(s2))], s2, where="mid", label="signal 2")
polish_plot(ax5)
ax5.plot(xs, f4_vals-f23_vals, label="f4-f23")

assert len(s1) == len(p1), "{} vs. {}".format(len(s1), len(p1))

# pos = ax.get_position()
# ax.set_position([pos.x0, pos.y0, pos.width, pos.height * 0.85])
# pos = ax.legend(loc="upper center", bbox_to_anchor=(0.5, 1.25), ncol=4)
plt.show()
