#!/bin/bash

echo "Building oscplayer for macOS..."

mkdir -p build/darwin
cd build/darwin
cmake -DCMAKE_BUILD_TYPE=Release ../..
make -j$(sysctl -n hw.ncpu)

if [ -f oscplayer ]; then
    echo "Build successful!"
    cp oscplayer ../../oscplayer-darwin-universal
    cp frame_timing.lua ../../
    echo "Copied to: ../../oscplayer-darwin-universal"
else
    echo "Build failed!"
fi
