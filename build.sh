#!/usr/bin/env nix-shell
#! nix-shell -i bash -p bash cmake
# cd to location of this script
cd "$(dirname "$0")"
if [ -z "$1" ]; then
  echo "Error: No argument provided."
  exit 1
fi
EMSDK="$1"
BUILD_DIR="build"
cmake -B $BUILD_DIR -DCMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake
cd $BUILD_DIR
make -j2
