# AGENTS.md — TTSG 项目代码说明

本文件面向 AI 编码代理（及新加入的开发者），说明 TTSG 项目的代码结构、核心机制和开发约定。

## 项目概述

TTSG（Time to say goodbye）是一个 Windows 桌面下班提醒小工具，由旧版 Delphi/Pascal 程序移植而来（部分注释中仍引用 `old_src/src/*.pas`、`*.lfm`）。

核心行为：

1. 从 Windows 事件日志读取当天考勤时间段内（默认 08:30–10:30）的最早开机时间，作为倒计时起点；找不到则以程序在该时间段内首次启动时间兜底。
2. 倒计时时长默认 8 小时，到点后弹出全屏提醒。
3. 桌面右下角常驻倒计时小组件；程序本体最小化到托盘（通知区域）。

## 技术栈

- **语言**：C++17，纯 Win32 API，无任何第三方库依赖。
- **构建**：CMake ≥ 3.20，支持 MSVC（Visual Studio 2022）和 MinGW（mingw-winlibs / GCC）双工具链，产物统一输出到仓库根目录 `out/`。
- **链接库**：仅 `user32` `gdi32` `shell32` `comctl32` `comdlg32` `advapi32`。
- **编译定义**：`UNICODE` `_UNICODE` `NOMINMAX` `WIN32_LEAN_AND_MEAN` `_WIN32_WINNT=0x0A00`（Windows 10+）。
- **所有源码为 UTF-8**；UI 文案为中文，集中在 `src/strings.h`。

## 构建命令

```bat
:: MinGW（PATH 中有其他编译器时需显式指定 g++/windres）
cmake -S . -B build\mingw -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ -DCMAKE_RC_COMPILER=windres
cmake --build build\mingw

:: Visual Studio
cmake -S . -B build\msvc -G "Visual Studio 17 2022" -A x64
cmake --build build\msvc --config Release
```

快速测试倒计时/提醒逻辑可用命令行参数（当天分钟数）：`out\ttsg.exe 0 1440 0` 会立即弹出下班提醒。

## 目录结构

```
CMakeLists.txt          构建脚本（版本号、双工具链适配、产物输出到 out/）
src/                    全部源码
  main.cpp              WinMain 入口：单实例互斥锁、初始化 CommonControls、启动 App
  app.h / app.cpp       应用控制器（见下）
  bootlog.h / bootlog.cpp  事件日志开机时间检测
  countdown.h / countdown.cpp  桌面倒计时小组件
  alert.h / alert.cpp   全屏下班提醒
  settingsdlg.h / settingsdlg.cpp  设置对话框
  util.h / util.cpp     DPI 辅助（GetUiDpi）
  strings.h             全部 UI 字符串（中文）、注册表键名、窗口类名
  resource.h            资源 ID / 控件 ID / 定时器 ID / WM_APP 消息定义
  ttsg.rc               资源脚本：图标、内嵌字体、manifest、VERSIONINFO（仅二进制+英文，无中文）
  app.manifest          comctl32 v6 视觉样式 + PerMonitorV2 DPI 感知 + asInvoker
  version.h.in          CMake configure_file 生成 version.h（由 TTSG_VERSION 注入）
assets/
  ttsg.ico              程序/托盘图标
  font.otf              倒计时用的内嵌字体（字体族名 "TTSG"）
out/                    编译产物（ttsg.exe）
.github/workflows/release.yml  打 tag 触发的双工具链构建与 GitHub Release 发布
```

## 核心模块说明

### App（src/app.h / app.cpp）— 应用控制器

- 创建一个**隐藏的主窗口**（普通顶级窗口而非 HWND_MESSAGE，以便接收 `WM_DISPLAYCHANGE`、`TaskbarCreated` 等广播消息），承载 1 秒定时器（`IDT_TICK`）和托盘图标。
- **开机时间状态机**：`Recompute()` 调用 `bootlog::FindSystemStartupTime()`；找到则计算 `goodbyeFT_`（开机时间 + 时长）并启用倒计时组件，未找到则禁用。每秒 `OnTick()` 更新剩余秒数，到点且未通知过时播放提示音并弹出提醒（`notified_` 防止重复弹窗）。`Recompute()` 会重置 `notified_`（参数变化后允许再次提醒）。
- **设置持久化**：注册表 `HKCU\Software\TTSG`，值名见 `strings.h`（`StartMinutes`/`EndMinutes`/`DurationMinutes`/`ShowCountdown`/`Message`/`BackgroundEnabled`/`BackgroundColor`）。分钟数一律 Clamp 到 0–1439，且要求 start < end，否则回落默认值。
- **开机自启**：写 `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` 下名为 `TTSG` 的值，内容为带引号的 exe 路径。
- **命令行兼容**：`ttsg.exe <开始分钟> <结束分钟> <时长分钟>`（旧版自启格式），优先级高于注册表设置。
- `ResetAll()`：删除整个 `HKCU\Software\TTSG` 键树及自启值，内存状态恢复默认。
- 单实例：命名互斥体 `Local\TTSG_Instance`（main.cpp）。

### bootlog（src/bootlog.h / bootlog.cpp）— 开机时间检测

- `LogAppStart()`：启动时向 **Application** 日志写入标记事件（源名 `TTSG`，事件 ID `65535`），作为"开始工作"的兜底时间戳。
- `FindSystemStartupTime(start, end)`：
  1. 首选扫描 **System** 日志中事件 ID `12`（Kernel-General 开机事件）；
  2. 未找到则扫描 **Application** 日志中 TTSG 自己的标记事件（需按源名过滤）。
  取当天本地时间落在 `[start, end]` 分钟区间内的**最早**一条记录。
- 实现要点：用 `ReadEventLogW` 从新到旧顺序读取，遇到早于区间起点的记录即提前终止；源名匹配需同时探测 UTF-16 与 ANSI 两种编码（`EVENTLOG_UNICODE_TYPE` 常量在 MS SDK 与 MinGW 中均未定义）。

### CountdownWindow（src/countdown.h / countdown.cpp）— 桌面倒计时组件

- 实现为**桌面的子窗口**：父窗口是 `Progman` 或 `WorkerW`（`FindDesktopHost()` 处理经典布局与 Win10/11 壁纸宿主布局，后者需向 Progman 发送 `0x052C` 消息触发）。因此它固定在桌面上、不遮挡任何顶级窗口、不受 Win+D 影响。
- 窗口样式：`WS_EX_LAYERED | WS_EX_TOOLWINDOW` 子窗口，黑底 + `LWA_COLORKEY` 透明（颜色键 `RGB(0,0,0)`），文字绘在黑底上。
- **自愈**：`App::OnTick()` 每秒调用 `EnsureCreated()`，explorer 重启/桌面刷新后自动重建窗口。
- 显示规则（`SetRemaining()` / `WM_PAINT`）：
  - 无开机时间：显示 `--:--`（`kNoRemaining` 哨兵值）；
  - 剩余 > 1 小时：白色 `时:分`；≤ 1 小时：黄色 `分:秒`；
  - 超时（负值）：负数加班时间，1 小时内红色 `-分:秒`，超过 1 小时紫色 `-时:分`。
- **可选背景色**（`SetBackground()`，设置持久化于 `BackgroundEnabled`/`BackgroundColor`）：关闭时维持纯透明；启用时用所选颜色 `RoundRect` 绘制圆角背景板，圆角外仍为颜色键透明。文字颜色按背景亮度自适应（浅色背景：标题白→黑、黄色→深琥珀；红/紫不变）。颜色键一般为纯黑，若用户选纯黑背景则改用 `RGB(0,0,1)` 作颜色键（`EffectiveColorKey()`），此时需销毁重建窗口才能生效——`SetBackground()` 内部已处理。
- 右键窗口 → 向主窗口发 `WM_APP_COUNT_CLOSED` 隐藏组件（可经托盘菜单恢复，显示意图持久化到 `ShowCountdown`）。
- 定位在主显示器工作区右下角，与右缘/任务栏保留 10 逻辑像素间隙（`kLogicalGap`）；坐标需减去虚拟屏原点（`SM_XVIRTUALSCREEN/Y`），因为桌面宿主坐标系以虚拟屏左上为原点（`BottomRightPosition()` 统一计算，创建与重定位共用）。`WM_DISPLAYCHANGE`/`WM_SETTINGCHANGE` 时重新定位。
- 字体：内嵌的 `assets/font.otf` 以 `RCDATA` 资源（名字 `TTSG_FONT`）打包，启动时 `AddFontMemResourceEx` 加载（缓冲区须常驻进程生命周期，故 `malloc` 不释放），字体族名 `TTSG`。
- 几何常量按 96 dpi 逻辑像素定义，绘制时经 `GetUiDpi()` 缩放。

### GoodbyeAlert（src/alert.h / alert.cpp）— 全屏下班提醒

- 两个 topmost 弹窗：覆盖整个虚拟屏（所有显示器）的 50% 透明黑色遮罩（常量 alpha 分层窗口），加居中于当前前台显示器的白色卡片（含确定按钮）。topmost 窗口无需管理员权限。
- `Show()` 内部跑**模态消息循环**直至卡片销毁；收到 `WM_QUIT` 会重新 `PostQuitMessage` 交还给外层循环，不能吞掉。
- 卡片尺寸随消息文本自适应（`DT_CALCRECT` 测量，宽度钳制在 [210, 480] 逻辑像素与显示器宽度 90% 之间）。
- 用 `AttachThreadInput` 挂接前台线程输入队列来抢占前台焦点；每秒定时器重申 topmost 层级。
- 提醒文案可在设置中自定义，空文案回落默认 `Time to say goodbye!`。

### SettingsDialog（src/settingsdlg.h / settingsdlg.cpp）— 设置对话框

- **纯代码构建控件，不使用 .rc 对话框模板**——这是为了规避 rc/windres 处理中文字符串的代码页问题。修改此对话框时不要试图引入 .rc 模板。
- 控件：两个时间选择器（考勤时间段起止）、一个时长选择器（均以"当天分钟数"与 App 交互）、提醒文案可编辑下拉框（内置文案见 `str::kBuiltinMsgs`）、开机自启复选框、倒计时组件背景色复选框 + 自绘色块按钮（`BS_OWNERDRAW`，带 tooltip；点击弹系统颜色选择器 `ChooseColorW`，未勾选背景色时色块 disabled；选择结果存工作副本，确定时经 `App::ApplyCountdownBackground()` 一并应用）、重置按钮（带 tooltip，二次确认）、确定/取消。下方实时显示检测到的开机时间与剩余时间（`IDT_DLG` 每秒刷新）。
- 模态单例：已打开时激活而非新建（`s_openDialog`）。同样自带模态消息循环（含加速键表，Enter/Escape 映射到确定/取消），同样不得吞 `WM_QUIT`。
- 需要 `InitCommonControlsEx` 的 `ICC_DATE_CLASSES`（main.cpp 已初始化）。

### util（src/util.h / util.cpp）

- `GetUiDpi()`：优先动态获取 `user32!GetDpiForSystem`（manifest 声明 PerMonitorV2），失败回落 `GetDeviceCaps(LOGPIXELSX)`。

## 重要约定与易错点

1. **中文字符串只放 `strings.h`（代码内宽字符串），绝不放进 `ttsg.rc`**——rc 文件只包含二进制资源与英文 VERSIONINFO，这是 rc.exe/windres 双工具链兼容的刻意设计。
2. **manifest 冲突**：应用清单由 `ttsg.rc`（资源 ID 1）内嵌，因此：
   - MSVC 侧必须 `/MANIFEST:NO`，否则链接器注入第二份清单导致 cvtres 报重复；
   - MinGW 侧 GCC 会通过 endfile spec 自动链接 `default-manifest.o`，CMakeLists 生成了一份剔除它的 specs 文件（`ttsg-no-default-manifest.specs`），改动链接选项时勿破坏此机制。
3. **版本号**：`TTSG_VERSION`（MAJOR.MINOR.PATCH 格式）经 CMake 注入 `version.h`，供 `ttsg.rc` 的 VERSIONINFO 使用。本地构建默认 `0.0.0`；CI 在打 `vX.Y.Z` tag 时以 tag 版本构建并发布（`release.yml` 还会校验 tag 必须位于 main 分支上）。
4. **时间运算**：统一使用 FILETIME（100ns 间隔）经 `ULARGE_INTEGER` 做加减；涉及"当天分钟"的参数一律先 `TodayLocalFileTime()` 转本地 FILETIME。注意 `SystemTimeToFileTime` 输入被视为本地时间。
5. **DPI**：所有布局常量按 96 dpi 逻辑像素定义，使用时 `MulDiv(v, dpi, 96)` 缩放；不要直接写物理像素。
6. **窗口过程模式**：所有窗口类都用同一套 `GWLP_USERDATA + WM_NCCREATE` 取实例指针的模式（静态 `WndProc` 转发到成员 `HandleMessage`），新增窗口请保持一致。
7. **单实例与托盘自愈**：托盘图标在收到 `TaskbarCreated` 广播消息时重新注册（explorer 重启）；倒计时窗口在每秒 tick 中自愈重建。相关逻辑勿删。
8. **注册表写入失败**：设置对话框会弹"写入注册表失败"；`ResetAll()` 对 `ERROR_FILE_NOT_FOUND` 视为成功（本来就没有）。
9. **修改启动参数语义时**：分钟数 Clamp 到 `[0, 1439]`，`end` 至少为 1，且必须 `start < end`；命令行与设置对话框两处都要满足这些约束。

## 发布流程

推送形如 `v0.1.1` 的 tag 触发 `.github/workflows/release.yml`：校验 tag 位于 main → MSVC 与 MinGW 分别构建 Release → 产物命名为 `ttsg-<tag>-<toolchain>.exe` → 创建 GitHub Release。
