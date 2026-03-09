#!/bin/bash

echo "Building oscplayer for Linux..."

mkdir -p build/linux
cd build/linux
cmake -DCMAKE_BUILD_TYPE=Release ../..
make -j$(nproc)

if [ -f oscplayer ]; then
    echo "Build successful!"
    cp oscplayer ../../oscplayer-linux
    chmod +x ../../oscplayer-linux
    cp frame_timing.lua ../../
    echo "Copied to: ../../oscplayer-linux"
else
    echo "Build failed!"
fi
