# OSCPlayer

https://github.com/ToCenTek/OSCVideoPlayer

一个基于 C++ 的 OSC (Open Sound Control) 协议控制视频播放器,使用 mpv 进行视频播放,支持 macOS/Linux/Windows。

## 功能特性

- **OSC 协议控制**: 通过 OSC 消息控制视频播放
- **外部 mpv 进程**: 使用系统 mpv 二进制进行视频播放
- **Unix 域套接字 IPC**: 通过 IPC 套接字与 mpv 通信
- **多平台支持**: 支持 macOS、Linux 和 Windows
- **通用二进制**: 同时支持 Intel (x86_64) 和 Apple Silicon (arm64) Mac
- **Zeroconf 服务发现**: 通过 Bonjour/Avahi 发布服务，支持 TouchOSC 等客户端自动发现

## 环境要求

- **mpv**: 必须在系统中安装 mpv
  - macOS: `brew install mpv`
  - Linux: `sudo apt install mpv` 或类似命令
  - Windows: 从 https://mpv.io/ 下载

## 安装使用

### 预编译版本 (macOS 通用二进制)

1. 下载 `oscplayer-darwin-universal`
2. 赋予执行权限: `chmod +x oscplayer-darwin-universal`
3. 运行 `./oscplayer-darwin-universal`

### 从源码构建 (所有平台)

#### 使用构建脚本 (推荐)

```bash
# 进入源码目录
cd /path/to/OSCPlayer

# 运行构建脚本 - 自动检测操作系统和架构
./build.sh
```

构建脚本会自动创建:

- **macOS**: `oscplayer-darwin-universal` (通用二进制: arm64 + x86_64)
- **Linux**: `oscplayer-linux-x86_64` 或 `oscplayer-linux-arm64`
- **Windows**: `oscplayer-windows-x86_64.exe`

#### 手动构建

**macOS:**

```bash
mkdir build && cd build
cmake .. -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
make
./oscplayer
```

**Linux:**

```bash
# 安装依赖
sudo apt install libavahi-client-dev libavahi-glib-dev

# 编译
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

### 依赖说明

| 平台      | 必需组件                                      |
| ------- | ----------------------------------------- |
| macOS   | `mpv` (`brew install mpv`)              |
| Linux   | `mpv`, `libavahi-client-dev`, `libavahi-glib-dev` |
| Windows | `mpv` (加入 PATH)                         |

## 使用方法

启动播放器:

```bash
./oscplayer-darwin-universal
```

服务器默认监听 `0.0.0.0:8000`。

## 视频搜索路径

1. **macOS:**
   
   - 搜索用户主目录: `Movies, Videos, Downloads, Desktop`
   - 搜索 `/Users/shared`
   - 搜索 `/Volumes` 下的可移动磁盘

2. **Linux:**
   
   - 搜索用户主目录:` Videos, Downloads, Desktop`
   - 搜索 `/home/shared`
   - 搜索` /media` 和 `/mnt `下的可移动设备

3. **Windows:**
   
   - 搜索用户目录: `Videos, Desktop, Downloads, Documents`
   - 搜索 `D~K` 盘符的根目录

4. **递归搜索:**
   
   - 从根目录向下搜索 `2` 层目录
   - 自动找出所有包含视频的子目录
   - 去除重复文件名

支持格式: `mp4, m4v, mov, avi, mkv, webm, mpg, mpeg`

## OSC 命令

### 播放控制

| 命令           | 说明                  | 示例                                     |
| ------------ | ------------------- | -------------------------------------- |
| `/play[xxx]` | 播放视频 (文件名或路径)       | `/play/hello.mp4` 或 `/play`            |
| `/stop[xxx]` | 停止在指定帧/时间或退出        | `/stop/100` 或 `/stop/exit`             |
| `/pause[x]`  | 切换暂停 (0=取消暂停, 1=暂停) | `/pause` 或 `/pause/1` 或 `/pause, 0`    |
| `/volume[x]` | 设置音量 (0.0-1.0)      | `/volume/0.5` 或 `/volume, 0.5`         |
| `/loop[x]`   | 启用/禁用循环 (0/1)       | `/loop/1` 或 `/loop, 1`                 |
| `/seek[x]`   | 跳转到位置               | `/seek/50` (50%), `/seek/10.5` (10.5秒) |

### 显示控制

| 命令               | 说明       | 示例                                           |
| ---------------- | -------- | -------------------------------------------- |
| `/fullscreen[x]` | 显示模式     | `0`=窗口, `1`=上下拉伸, `2`=左右拉伸, `3`=全屏           |
| `/display[x]`    | 设置显示器    | `/display/0` 或 `/display, "显示器名称"`           |
| `/tct[文本/大小/位置]` | 在屏幕上显示文本 | `/tct/DEMO/48/0` 或 `/tct, "DEMO", "48", "0"` |

位置值: `0`=居中, `1`=左上, `2`=右上, `3`=左下, `4`=右下

### 音频控制

| 命令            | 说明     | 示例                            |
| ------------- | ------ | ----------------------------- |
| `/audio[x]`   | 设置音频设备 | `/audio/0` 或 `/audio, "设备名称"` |
| `/list/audio` | 列出音频设备 | `/list/audio`                 |

### 系统命令

| 命令             | 说明                    |
| -------------- | --------------------- |
| `/fps[x]`      | FPS 显示模式 (0=关闭, 1=开启) |
| `/port[名称/端口]` | 设置回复端口                |
| `/reboot`      | 重新启动系统                |
| `/shutdown`    | 关闭系统                  |
| `/help`        | 显示帮助信息                |

### 列表命令

| 命令              | 说明                     |
| --------------- | ---------------------- |
| `/list/audio`   | 列出音频设备 → `/AudioList`  |
| `/list/display` | 列出显示器 → `/DisplayList` |
| `/list/videos`  | 列出所有视频 → `/Videos`     |

## 命令格式

支持多种格式:

### 路径格式

```
/命令/参数
```

### OSC 参数格式

```
/命令, 参数1, 参数2, 参数3
```

### 混合格式

```
/命令, "参数"
```

### 示例

```bash
# 播放视频
/play/hello.mp4
/play
/play, "hello"
/play, "hello.mp4"

# 停止/退出
/stop/exit
/stop, 0

# 暂停
/pause
/pause/1
/pause, 0

# 音量
/volume/0.5
/volume, 0.5

# 全屏
/fullscreen/3
/fullscreen, 3

# 显示文本
/tct/DEMO/48/0
/tct, "DEMO", "48", "0"
/tct, "demo/48/0"
/tct/0 (关闭)

# 设置回复端口
/port/chataigne/12000
/port, "chataigne", 12000
/port/12000
/port, 12000

# 列表
/list/videos
/list/audio
/list/display

# 帮助
/help
```

## 响应消息

| 响应             | 说明      |
| -------------- | ------- |
| `/Playing`     | 视频已开始   |
| `/Stopped`     | 视频已停止   |
| `/Pause`       | 暂停状态    |
| `/Volume`      | 音量已设置   |
| `/Loop`        | 循环状态    |
| `/Fullscreen`  | 全屏模式已设置 |
| `/SeekTo`      | 跳转结果    |
| `/Audio`       | 音频设备已设置 |
| `/AudioList`   | 音频设备列表  |
| `/Display`     | 显示器已设置  |
| `/DisplayList` | 显示器列表   |
| `/Videos`      | 视频列表    |
| `/FPS`         | FPS 信息  |
| `/TCT`         | 文本显示状态  |
| `/OSCPlayer`   | 端口设置确认  |
| `/Help`        | 帮助文本    |
| `/Error`       | 错误消息    |
| `/Unknown`     | 未知命令    |

## Zeroconf 服务发现

播放器启动时会自动通过 Zeroconf 发布服务，使客户端（如 TouchOSC）可以自动发现服务器。

| 平台   | 服务类型        | 服务名称              |
| ---- | ----------- | ----------------- |
| macOS | `_osc._udp.` | `OSCPlayer - [主机名]` |
| Linux | `_osc._udp.` | `OSCPlayer - [主机名]` |
| Windows | `_osc._udp.` | `OSCPlayer - [主机名]` |

**端口**: 8000

### TouchOSC 发现服务

1. 确保客户端和播放器在同一网络
2. 在 TouchOSC 的连接设置中启用 "Auto"
3. 客户端应能发现 `OSCPlayer - [主机名]` 服务

## 架构图

```
┌─────────────────┐     OSC      ┌─────────────────┐
│  控制应用       │ ───────────► │    OSCPlayer    │
│  (Chataigne)    │ ◄────────── │   (服务器 :8000) │
└─────────────────┘              └────────┬────────┘
                                          │
                                          │ IPC
                                          ▼
                                   ┌─────────────────┐
                                   │  mpv (子进程)   │
                                   │ (套接字控制)    │
                                   └─────────────────┘
```

## 文件结构

```
OSCPlayer/
├── CMakeLists.txt
├── README.md
├── README_cn.md
├── build.sh                     (跨平台构建脚本)
├── oscplayer-darwin-universal   (预编译 macOS 二进制)
├── oscplayer-linux-*             (Linux 二进制)
├── oscplayer-windows-*           (Windows 二进制)
├── frame_timing.lua             (mpv 帧时间脚本)
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

## 许可证

MIT License

## 版本

1.0.1
