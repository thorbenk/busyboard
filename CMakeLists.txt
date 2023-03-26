cmake_minimum_required(VERSION 3.13)
include(3rdparty/pico-sdk/pico_sdk_init.cmake)
project(busyboard LANGUAGES C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
set(PICO_CXX_ENABLE_EXCEPTIONS 1)
set(PICO_CXX_ENABLE_RTTI 1)

pico_sdk_init()

include("3rdparty/PicoLed/PicoLed.cmake")
include("3rdparty/TM1637-pico/PicoTM1637.cmake")
include("3rdparty/pico-ads1115/lib/CMakeLists.txt")

add_executable(busyboard 
  busyboard.cpp 
  debounce.h
  debounce.cpp
  color.h
  color.cpp
  sound_game.h
  sound_game.cpp
  phone.h
  phone.cpp
  fan_leds.h
  fan_leds.cpp
  modes.h
  dotmatrix.h 
  dotmatrix.cpp
  3rdparty/Pico7219/pico7219/src/pico7219.c
  3rdparty/raster-fonts/font-8x8.c
)
target_include_directories(busyboard PRIVATE
  ${PROJECT_SOURCE_DIR}/3rdparty/Pico7219/pico7219/include 
  ${PROJECT_SOURCE_DIR}/3rdparty/pico-dfPlayer
  ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(busyboard 
  pico_stdlib  
  hardware_i2c
  hardware_spi
  hardware_pwm
  PicoLed
  pico-ads1115
)
#pico_add_extra_outputs(busyboard)
#pico_enable_stdio_usb(busyboard 1)
#pico_enable_stdio_uart(busyboard 0)

add_subdirectory(parts)
