cmake_minimum_required(VERSION 3.13)
include(3rdparty/pico-sdk/pico_sdk_init.cmake)
project(busyboard LANGUAGES C CXX ASM)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(analog_dial)
