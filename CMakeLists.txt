cmake_minimum_required(VERSION 3.13)
include(3rdparty/pico-sdk/pico_sdk_init.cmake)
project(busyboard LANGUAGES C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)
set(PICO_CXX_ENABLE_RTTI 1)

pico_sdk_init()

include("3rdparty/PicoLed/PicoLed.cmake")

add_subdirectory(neopixel_tests)
add_subdirectory(analog_dial)
add_subdirectory(vid28-05)
add_subdirectory(arcade_rgb_button)
