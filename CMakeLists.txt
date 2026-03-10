cmake_minimum_required(VERSION 3.15)
project(OSCPlayer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -framework CoreFoundation -framework CoreAudio")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()

set(SOURCES
    src/main.cpp
    src/Player.cpp
    src/DisplayInfo.cpp
    src/AudioDevice.cpp
    src/OSCServer.cpp
    src/OSCMessage.cpp
    src/Platform.cpp
    src/Zeroconf.cpp
)

set(HEADERS
    include/Player.h
    include/DisplayInfo.h
    include/AudioDevice.h
    include/OSCServer.h
    include/OSCMessage.h
    include/Platform.h
    include/Zeroconf.h
)

add_executable(oscplayer ${SOURCES} ${HEADERS})

add_custom_command(TARGET oscplayer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/frame_timing.lua
        ${CMAKE_BINARY_DIR}/frame_timing.lua
    COMMENT "Copying frame_timing.lua to build directory"
)

target_link_libraries(oscplayer
    pthread
)

if(APPLE)
    target_link_libraries(oscplayer "-framework CoreFoundation")
endif()

if(WIN32)
    target_link_libraries(oscplayer ws2_32 dnsapi)
endif()

# Linux - Avahi
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(AVAHIGLIB QUIET avahi-glib avahi-client)
    endif()
    if(AVAHIGLIB_FOUND)
        add_compile_definitions(HAVE_AVAHI=1)
        target_link_libraries(oscplayer ${AVAHIGLIB_LIBRARIES})
        include_directories(${AVAHIGLIB_INCLUDE_DIRS})
        link_directories(${AVAHIGLIB_LIBRARY_DIRS})
        message(STATUS "Avahi support enabled")
    else()
        message(STATUS "Avahi not found, Zeroconf will be disabled")
    endif()
endif()
