include("${PROJECT_SOURCE_DIR}/3rdparty/TM1637-pico/PicoTM1637.cmake")

add_executable(analog_dial analog_dial.cpp)
target_include_directories(analog_dial PRIVATE "${PROJECT_SOURCE_DIR}/3rdparty/pico-dfPlayer")
target_link_libraries(analog_dial pico_stdlib hardware_adc hardware_pwm PicoTM1637 PicoLed)
pico_enable_stdio_usb(analog_dial 0)
pico_enable_stdio_uart(analog_dial 1)
