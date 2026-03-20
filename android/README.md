# OSCVideoPlayer

Android TV 视频播放器，通过 OSC 命令控制。

## 功能

- OSC 命令控制播放
- 自动循环播放（可通过 `/loop/0` 关闭）
- 视频扫描（内置存储 + USB）
- 最小化 UI（自动全屏播放）

## OSC 命令

| 命令 | 说明 |
|------|------|
| `/play[文件名]` | 播放视频（可选文件名） |
| `/stop` 或 `/stop/时间` | 停止播放，可指定时间（秒）并暂停 |
| `/pause` 或 `/pause/0/1` | 切换暂停状态 |
| `/volume/0.0-1.0` | 设置音量 |
| `/loop/0` | 关闭循环 |
| `/loop/1` 或 `/loop` | 开启循环 |
| `/seek/秒数` | 跳转到指定时间（秒） |
| `/tct/文字/字号/位置` | 显示文字叠加 |
| `/list/videos` | 列出视频 |
| `/port/名称/端口` | 设置回复端口 |
| `/help` | 显示帮助 |

## 构建

```bash
cd android
./gradlew assembleDebug
```

APK 位置: `android/app/build/outputs/apk/debug/app-debug.apk`

## 安装

```bash
adb connect <电视IP>:5555
adb install app-debug.apk
```

## 默认行为

- 所有视频默认全屏循环播放
- `/loop/0` 可关闭循环
- 视频扫描目录：
  - 内置存储（Movies, Downloads, DCIM）
  - USB 存储
- 启动时播放第一个名为 "hello.*" 的视频

## 遥控器按键

- **菜单键** - 打开搜索/文件管理或关于页面
- **返回键** - 退出应用

## 稳定性

- App 会尽量保持持久运行
- OSC 服务器会在后台持续运行
