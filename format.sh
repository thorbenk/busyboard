find parts/ -name \*.h -or -name \*.cpp | xargs clang-format -i
clang-format -i debounce.h
clang-format -i debounce.cpp
clang-format -i busyboard.cpp
find hardware/ -name \*.py | xargs black
