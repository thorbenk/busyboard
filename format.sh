find parts/ -name \*.h -or -name \*.cpp | xargs clang-format -i
clang-format -i *.h
clang-format -i *.cpp
find hardware/ -name \*.py | xargs black
