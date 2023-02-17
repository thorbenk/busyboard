add_executable(arcade_rgb_button arcade_rgb_button.cpp ../debounce.cpp)
target_include_directories(arcade_rgb_button PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(arcade_rgb_button pico_stdlib PicoLed)
