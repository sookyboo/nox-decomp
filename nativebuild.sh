mkdir -p build
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build build -j 2>&1 | tee build.log

echo "BUILD ERRORS:"
grep -n -E -C2 '(^|: )error:|undefined reference|collect2: error|ld: .*error|ninja: build stopped' build.log