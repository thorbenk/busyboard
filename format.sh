clang-format -i analog_dial/analog_dial.cpp
clang-format -i analog_dial/dfPlayerDriver.h
clang-format -i vid28-05/motor_vid28.cpp
clang-format -i arcade_rgb_button/arcade_rgb_button.cpp
clang-format -i debounce.h
clang-format -i debounce.cpp
find hardware/ -name \*.py | xargs black
