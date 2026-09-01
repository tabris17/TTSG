# TTSG (Time To Say Goodbye)

显示下班倒计时的 Windows 桌面小工具。

## 原理

从 Windows 事件日志中读取特定时间段内（默认 8:30 ~ 10:30）的最早电脑开机时间作为倒计时的起始时间。如果未找到，则以程序在该时间段内的第一次启动时间为起始时间。倒计时长默认 8 小时，可在设置中修改。

## 功能

- 桌面右下角固定显示倒计时（不遮挡其他窗口、不受 Win+D 影响、桌面刷新后自动重建，与屏幕右缘/任务栏保留 10 像素间隙）
  - 剩余 1 小时以上显示 `时:分`（白色），不足 1 小时切换为 `分:秒`（黄色）
  - 倒计时结束后改为显示负的加班时间（红色）：加班 1 小时内显示 `-分:秒`，超过 1 小时显示 `-时:分`
  - 可选圆角背景色：默认关闭（保持透明样式）；启用后填充背景色，颜色可用系统颜色选择器自定义，浅色背景下文字颜色自动适配
  - 右键倒计时窗口可隐藏，通过托盘菜单重新显示
- 倒计时结束后弹窗提醒下班
- 托盘图标：打开设置、显示/隐藏倒计时、退出
- 设置窗口：考勤时间段、倒计时时长、开机自启、倒计时组件背景色开关与颜色选择，并显示开机时间与实时倒计时；左下角"重置"按钮可一键清除所有注册表设置（`HKCU\Software\TTSG` 及开机自动启动项）并恢复默认值
- 设置保存在注册表 `HKCU\Software\TTSG`

## 构建

要求：CMake ≥ 3.20，Visual Studio 2022 或 mingw-winlibs（GCC）。编译产物输出到 `out/`。

MinGW（PATH 中若存在其他编译器，需显式指定 mingw-winlibs 的 g++/windres）：

```bat
cmake -S . -B build\mingw -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DCMAKE_RC_COMPILER=windres
cmake --build build\mingw
```

Visual Studio：

```bat
cmake -S . -B build\msvc -G "Visual Studio 17 2022" -A x64
cmake --build build\msvc --config Release
```

## 命令行参数

`ttsg.exe <开始分钟> <结束分钟> <时长分钟>`（均为当天分钟数，兼容旧版自启格式，便于测试）。
例如 `ttsg.exe 0 1440 0` 会立即弹出下班提醒。
