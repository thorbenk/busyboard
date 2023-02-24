#!/usr/bin/bash

W=$HOME/local/share/openocd/scripts

openocd -f $W/interface/picoprobe.cfg -f $W/target/rp2040.cfg \
  -c "adapter speed 5000" \
  -c "targets rp2040.core0; program build/parts/arcade_rgb_button/arcade_rgb_button.elf verify reset exit"
#  -c "targets rp2040.core0; program build/parts/seven_segment_display/seven_segment_display.elf verify reset exit"
#  -c "targets rp2040.core0; program build/parts/arcade_rgb_button/arcade_rgb_button.elf verify reset exit"
#  -c "targets rp2040.core0; program build/parts/vid28-05/motor_vid28.elf verify reset exit"
#  -c "targets rp2040.core0; program build/parts/analog_dial/analog_dial.elf verify reset exit"
#  -c "targets rp2040.core0; program build/parts/neopixel_tests/led_example.elf verify reset exit"
