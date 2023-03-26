find parts/ -name \*.h -or -name \*.cpp | xargs clang-format -i
clang-format -i debounce.h
clang-format -i debounce.cpp
clang-format -i busyboard.cpp
clang-format -i color.h
clang-format -i sound_game.h
clang-format -i sound_game.cpp
clang-format -i arcade_sounds.h
find hardware/ -name \*.py | xargs black
