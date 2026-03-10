@echo off

if not exist build mkdir build

set SRC=src\Player.cpp src\DisplayInfo.cpp src\Platform.cpp src\OSCServer.cpp src\OSCMessage.cpp src\AudioDevice.cpp src\Zeroconf.cpp src\main.cpp

echo Compiling...

REM Create import library for dnssd.dll if not exists
if not exist libdnssd.a (
    echo Creating libdnssd.a...
    if not exist temp mkdir temp
    (echo EXPORTS & echo DNSServiceAddRecord & echo DNSServiceBrowse & echo DNSServiceConstructFullName & echo DNSServiceCreateConnection & echo DNSServiceEnumerateDomains & echo DNSServiceGetAddrInfo & echo DNSServiceGetProperty & echo DNSServiceNATPortMappingCreate & echo DNSServiceProcessResult & echo DNSServiceQueryRecord & echo DNSServiceReconfirmRecord & echo DNSServiceRefDeallocate & echo DNSServiceRefSockFD & echo DNSServiceRegister & echo DNSServiceRegisterRecord & echo DNSServiceRemoveRecord & echo DNSServiceResolve & echo DNSServiceUpdateRecord) > temp\dnssd.def
    C:\msys64\mingw64\bin\dlltool.exe -D C:\Windows\System32\dnssd.dll -d temp\dnssd.def -l libdnssd.a
)

REM Compile with Bonjour support using system dnssd.dll
C:\msys64\mingw64\bin\g++ -O2 -std=c++17 -mconsole -Iinclude -DHAVE_BONJOUR=1 %SRC% -o build\oscplayer.exe -lws2_32 -lwinmm libdnssd.a

if exist build\oscplayer.exe (
    echo OK
    copy build\oscplayer.exe oscplayer-windows-x86_64.exe
) else (
    echo FAIL
)

pause
