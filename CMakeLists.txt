cmake_minimum_required(VERSION 3.13)
include(3rdparty/pico-sdk/pico_sdk_init.cmake)
project(busyboard LANGUAGES C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)
set(PICO_CXX_ENABLE_RTTI 1)

pico_sdk_init()

include("3rdparty/PicoLed/PicoLed.cmake")
include("3rdparty/TM1637-pico/PicoTM1637.cmake")

add_executable(busyboard 
  busyboard.cpp 
  debounce.cpp
)
target_link_libraries(busyboard pico_stdlib hardware_i2c)
pico_add_extra_outputs(busyboard)

add_subdirectory(parts)
