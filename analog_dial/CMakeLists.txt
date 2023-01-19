include("../3rdparty/TM1637-pico/PicoTM1637.cmake")

add_executable(analog_dial analog_dial.cpp)
target_link_libraries(analog_dial pico_stdlib PicoTM1637)
pico_enable_stdio_usb(analog_dial 0)
pico_enable_stdio_uart(analog_dial 1)
