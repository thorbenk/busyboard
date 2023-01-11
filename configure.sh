mkdir -p build
cd build
cmake -GNinja -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=1 ..
ln -sfn $PWD/compile_commands.json ../
