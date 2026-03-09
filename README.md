# OSCPlayer

A C++ OSC (Open Sound Control) controlled video player using mpv for video playback on macOS/Linux/Windows.

## Features

- **OSC Protocol Control**: Control video playback via OSC messages
- **External mpv Process**: Uses system mpv binary for video playback
- **Unix Domain Socket IPC**: Communicates with mpv via IPC socket
- **Multi-platform Support**: Works on macOS, Linux, and Windows
- **Universal Binary**: Supports both Intel (x86_64) and Apple Silicon (arm64) Macs

## Requirements

- **mpv**: Must be installed on the system
  - macOS: `brew install mpv`
  - Linux: `sudo apt install mpv` or similar
  - Windows: Download from https://mpv.io/

## Installation

### Pre-built (macOS Universal Binary)

1. Download `oscplayer-darwin-universal` (supports both Intel and Apple Silicon)
2. Make executable: `chmod +x oscplayer-darwin-universal`
3. Run:  `./oscplayer-darwin-universal`

### Build from Source (All Platforms)

#### Using Build Script (Recommended)

```bash
# Clone or download source code
cd /path/to/OSCPlayer

# Run the build script - auto-detects OS and architecture
./build.sh
```

The build script will create:

- **macOS**: `oscplayer-darwin-universal` (Universal Binary: arm64 + x86_64)
- **Linux**: `oscplayer-linux-x86_64` or `oscplayer-linux-arm64`
- **Windows**: `oscplayer-windows-x86_64.exe`

#### Manual Build

**macOS:**

```bash
mkdir build && cd build
cmake .. -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
make
./oscplayer
```

**Linux:**

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make
./oscplayer
```

**Windows:**

```bash
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make
oscplayer.exe
```

### Dependencies

| Platform | Required                                       |
| -------- | ---------------------------------------------- |
| macOS    | `mpv` (`brew install mpv`)                     |
| Linux    | `mpv`, `pulseaudio` (for audio device listing) |
| Windows  | `mpv` (add to PATH)                            |

## Usage

Start the player:

```bash
./oscplayer-darwin-universal
```

The server listens on `0.0.0.0:8000` by default.

## Video Search Paths

Videos are searched in the following directories (in order):

1. `/Users/%USER%/OSCPlayer_v1.0/go/media`
2. `/Users/%USER%/Movies`
3. `/Users/%USER%/Videos`
4. `/Users/%USER%/Downloads`
5. `/Users/shared/Movies`
6. `/Users/shared/Videos`

Supported formats: mp4, m4v, mov, avi, mkv, webm, mpg, mpeg

## OSC Commands

### Playback Control

| Command      | Description                       | Example                                |
| ------------ | --------------------------------- | -------------------------------------- |
| `/play[xxx]` | Play video (filename or path)     | `/play/hello.mp4` or `/play`           |
| `/stop[xxx]` | Stop at frame/time or exit        | `/stop/100` or `/stop/exit`            |
| `/pause[x]`  | Toggle pause (0=unpause, 1=pause) | `/pause` or `/pause/1` or `/pause, 0`  |
| `/volume[x]` | Set volume (0.0-1.0)              | `/volume/0.5` or `/volume, 0.5`        |
| `/loop[x]`   | Enable/disable loop (0/1)         | `/loop/1` or `/loop, 1`                |
| `/seek[x]`   | Seek to position                  | `/seek/50` (50%), `/seek/10.5` (10.5s) |

### Display Control

| Command               | Description         | Example                                                  |
| --------------------- | ------------------- | -------------------------------------------------------- |
| `/fullscreen[x]`      | Display mode        | `0`=window, `1`=v-stretch, `2`=h-stretch, `3`=fullscreen |
| `/display[x]`         | Set display         | `/display/0` or `/display, "display name"`               |
| `/tct[text/size/pos]` | Show text on screen | `/tct/DEMO/48/0` or `/tct, "DEMO", "48", "0"`            |

Position values: `0`=center, `1`=top-left, `2`=top-right, `3`=bottom-left, `4`=bottom-right

### Audio Control

| Command       | Description        | Example                               |
| ------------- | ------------------ | ------------------------------------- |
| `/audio[x]`   | Set audio device   | `/audio/0` or `/audio, "device name"` |
| `/list/audio` | List audio devices | `/list/audio`                         |

### System Commands

| Command            | Description                    |
| ------------------ | ------------------------------ |
| `/fps[x]`          | FPS display mode (0=off, 1=on) |
| `/port[name/port]` | Set reply port                 |
| `/reboot`          | Reboot system                  |
| `/shutdown`        | Shutdown system                |
| `/help`            | Show help                      |

### List Commands

| Command         | Description                       |
| --------------- | --------------------------------- |
| `/list/audio`   | List audio devices → `/AudioList` |
| `/list/display` | List displays → `/DisplayList`    |
| `/list/videos`  | List all videos → `/Videos`       |

## Command Format

Commands support multiple formats:

### Path Format

```
/command/param
```

### OSC Arguments Format

```
/command, arg1, arg2, arg3
```

### Mixed Format

```
/command, "param"
```

### Examples

```bash
# Play video
/play/hello.mp4
/play
/play, "hello"
/play, "hello.mp4"

/# Stop/exit
/stop/exit
/stop, 0

# Pause
/pause
/pause/1
/pause, 0

# Volume
/volume/0.5
/volume, 0.5

# Fullscreen
/fullscreen/3
/fullscreen, 3

# Show text
/tct/DEMO/48/0
/tct, "DEMO", "48", "0"
/tct, "demo/48/0"
/tct/0 (close)

# Set reply port
/port/chataigne/12000
/port, "chataigne", 12000
/port/12000
/port, 12000

# List
/list/videos
/list/audio
/list/display

# Help
/help
```

## Response Messages

| Response       | Description           |
| -------------- | --------------------- |
| `/Playing`     | Video started         |
| `/Stopped`     | Video stopped         |
| `/Pause`       | Pause state           |
| `/Volume`      | Volume set            |
| `/Loop`        | Loop state            |
| `/Fullscreen`  | Fullscreen mode set   |
| `/SeekTo`      | Seek result           |
| `/Audio`       | Audio device set      |
| `/AudioList`   | Audio device list     |
| `/Display`     | Display set           |
| `/DisplayList` | Display list          |
| `/Videos`      | Video list            |
| `/FPS`         | FPS info              |
| `/TCT`         | Text display state    |
| `/OSCPlayer`   | Port set confirmation |
| `/Help`        | Help text             |
| `/Error`       | Error message         |
| `/Unknown`     | Unknown command       |

## Architecture

```
┌─────────────────┐     OSC      ┌─────────────────┐
│  Control App    │ ───────────► │    OSCPlayer    │
│  (Chataigne)    │ ◄────────── │   (Server :8000)│
└─────────────────┘              └────────┬────────┘
                                          │
                                          │ IPC
                                          ▼
                                   ┌─────────────────┐
                                   │  mpv (child)    │
                                   │ (socket ctrl)   │
                                   └─────────────────┘
```

## File Structure

```
OSCPlayer/
├── CMakeLists.txt
├── README.md
├── build.sh                     (build script for all platforms)
├── oscplayer-darwin-universal   (pre-built macOS binary)
├── oscplayer-linux-*            (Linux binaries)
├── oscplayer-windows-*          (Windows binary)
├── frame_timing.lua             (mpv script for frame timing)
├── include/
│   ├── Player.h
│   ├── OSCServer.h
│   ├── OSCMessage.h
│   ├── DisplayInfo.h
│   └── AudioDevice.h
└── src/
    ├── main.cpp
    ├── Player.cpp
    ├── OSCServer.cpp
    ├── OSCMessage.cpp
    ├── DisplayInfo.cpp
    └── AudioDevice.cpp
```

## License

MIT License

## Version

1.0.1
