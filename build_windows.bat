@echo off

if not exist build mkdir build

set SRC=src\Player.cpp src\DisplayInfo.cpp src\Platform.cpp src\OSCServer.cpp src\OSCMessage.cpp src\AudioDevice.cpp src\Zeroconf.cpp src\main.cpp

echo Compiling...

C:\msys64\mingw64\bin\g++ -O2 -std=c++17 -mconsole -Iinclude -DHAVE_BONJOUR=1 %SRC% -o build\oscplayer.exe -lws2_32 -lwinmm -ldnssd

if exist build\oscplayer.exe (
    echo OK
    copy build\oscplayer.exe oscplayer-windows-x86_64.exe
) else (
    echo FAIL
)

pause
