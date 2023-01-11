cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(busyboard LANGUAGES C CXX ASM)

pico_sdk_init()

include("../3rdparty/TM1637-pico/PicoTM1637.cmake")

add_executable(analog_dial analog_dial.c)
target_link_libraries(analog_dial pico_stdlib PicoTM1637)
pico_add_extra_outputs(analog_dial)
pico_enable_stdio_usb(analog_dial 1)
pico_enable_stdio_uart(analog_dial 0)
