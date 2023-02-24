add_executable(arcade_rgb_button  
  arcade_rgb_button.cpp
  ${PROJECT_SOURCE_DIR}/debounce.cpp
  ${PROJECT_SOURCE_DIR}/color.cpp
)
target_include_directories(arcade_rgb_button PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(arcade_rgb_button pico_stdlib PicoLed)
