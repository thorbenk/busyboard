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
  color.cpp
  3rdparty/Pico7219/pico7219/src/pico7219.c
  3rdparty/Pico7219/test/font8.c
)
target_include_directories(busyboard PRIVATE
  3rdparty/Pico7219/pico7219/include 
)
target_link_libraries(busyboard 
  pico_stdlib  
  hardware_i2c
  hardware_spi
  PicoLed
)

add_subdirectory(parts)
