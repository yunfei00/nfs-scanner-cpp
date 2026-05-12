# NFS Scanner C++

NFS Scanner C++ 是近场扫描系统的 C++17 / Qt 6 Widgets 重构工程。当前阶段聚焦于可运行、可演示、可继续扩展的桌面主界面、运动控制和扫描流程控制，暂不接入真实频谱仪、相机或 OpenCV。

## 当前版本

### v0.8.0 频谱采集稳定性版本

- 新增 SpectrumAcquisitionWorker。
- 扫描流程支持异步频谱采集。
- 支持采集超时、重试和失败停止策略。
- 新增 R&S FSW 初版 TCP SCPI 适配。
- 新增 Keysight N9020A 初版 TCP SCPI 适配。
- 保留 Mock Spectrum fallback。

### v0.7.0 频谱仪框架版本

- 新增 ISpectrumAnalyzer 统一接口。
- 新增 ScpiTcpClient。
- 新增 Generic SCPI 频谱仪。
- 新增 R&S ZNA67 初版适配。
- 支持 TCP SCPI 连接、查询 IDN、配置频率范围、单次扫描。
- 扫描流程支持使用真实频谱仪数据写入 `traces.csv`。
- 保留 Mock Spectrum，支持无仪表演示。

### v0.6.0 热力图增强版本

- 支持 LUT 选择。
- 支持自动/手动 vmin/vmax。
- 支持透明度控制。
- 支持 Colorbar。
- 支持主界面 HeatmapView 显示。
- 支持热力图弹窗预览和 PNG 导出。

### v0.5.0 离线分析版本

- 支持加载 `traces.csv`。
- 支持自动发现 Trace。
- 支持频率点选择。
- 支持幅度、幅度dB、相位、实部、虚部显示模式。
- 支持生成基础热力图。
- 支持导出热力图 PNG。

### v0.4.0 数据存储版本

- 扫描开始时创建任务目录。
- 保存 `meta.json`。
- 保存 `scan_config.json`。
- 保存 `points.csv`。
- 保存 `traces.csv`。
- 使用 MockSpectrumAnalyzer 生成模拟频谱数据。
- `traces.csv` 兼容旧 Python 频率文件格式。
- 扫描完成后可以点击查看打开任务目录。

### v0.3.0 扫描流程版本

- 新增 ScanConfig / ScanPoint / ScanPathPlanner。
- 支持扫描区域读取。
- 支持蛇形路径规划。
- 支持扫描状态机。
- 支持开始、暂停、继续、停止。
- 支持扫描进度、剩余点数、预计完成时间显示。
- 当前阶段仅模拟扫描，不采集真实频谱，不保存数据。

### v0.2.0 运动控制版本

- 新增 Qt SerialPort 真实串口接入。
- 支持刷新串口、打开/关闭串口。
- 支持 `$H`、`?`、`$I`、`$`、`G1X..Y..Z..F..`。
- 支持 GRBL-like MPos 状态解析。
- 支持 X/Y/Z 点动与绝对坐标移动。
- 支持坐标范围保护。
- 保留模拟模式，方便无硬件演示。

### v0.1.1 UI 对齐版本

- 完成接近 Python 版 NFS Scanner 的主界面布局。
- 支持串口 mock、运动控制 mock、扫描区域配置、仪表配置占位、结果区、日志区、状态栏。
- 暂未接入真实硬件。
- 下一阶段计划接入 Qt SerialPort 运动控制模块。

## 项目目标

- 使用 C++17 和 Qt 6 Widgets 重构近场扫描系统客户端。
- 以 CMake 作为跨平台构建入口，支持 Windows 和 Linux。
- 当前阶段提供工业测试软件风格的主界面、Mock 设备发现、Mock 运动控制、扫描任务存储、离线分析和热力图增强显示。
- 后续逐步接入更多真实频谱仪、数据采集线程、报告导出和版本签名。

## 依赖

- C++17 兼容编译器
  - Windows: MSVC 2022 或 MinGW-w64
  - Linux: GCC 10+ 或 Clang 12+
- Qt 6，至少包含 Core、Gui、Widgets、SerialPort、Network 模块
- CMake 3.20+
- 发布安装包需要 Inno Setup 6

## Windows 一键配置 Qt + 构建

推荐命令：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/dev_build_all_windows.ps1
```

分步命令：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/setup_qt_windows.ps1
powershell -ExecutionPolicy Bypass -File scripts/build_windows_msvc.ps1
powershell -ExecutionPolicy Bypass -File scripts/run_windows_msvc.ps1
```

默认会通过 `aqtinstall` 安装 Qt `6.8.3` 的 `win64_msvc2022_64` 包和 `qtserialport` 模块到 `C:/Qt`，构建脚本使用的 Qt 路径为 `C:/Qt/6.8.3/msvc2022_64`。如果 Qt 已经安装，安装脚本会复用现有目录并补齐 Qt SerialPort。

## 本地构建

### Windows: Visual Studio 2022 + Qt MSVC

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build --config Release
.\build\Release\NFSScanner.exe
```

也可以使用项目脚本：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_windows_msvc.ps1 -QtPath "C:/Qt/6.8.3/msvc2022_64"
powershell -ExecutionPolicy Bypass -File scripts/run_windows_msvc.ps1
```

如果 CMake 没有加入 PATH，但 Visual Studio 或独立 CMake 已安装到其他位置，可以额外传入：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_windows_msvc.ps1 -QtPath "C:/Qt/6.8.3/msvc2022_64" -CMakePath "C:/Program Files/CMake/bin/cmake.exe"
```

### Windows: MinGW + Qt MinGW

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64"
cmake --build build --config Release
.\build\NFSScanner.exe
```

### Linux

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/Qt/6.8.3/gcc_64
cmake --build build -j
./build/NFSScanner
```

如果 Qt 已通过系统包管理器安装且 CMake 可以自动找到 Qt6，可以省略 `CMAKE_PREFIX_PATH`。

## 当前功能

- 主窗口标题为 `NFS Scanner v0.8.0 - 近场扫描系统`，默认窗口大小 1600 x 900。
- 左右两栏布局：左侧为串口设置、运动控制、运动命令、步长设置、测试说明和功能操作区；右侧为扫描区域、仪表区域、结果区域和日志区域。
- 串口控制：默认模拟模式；取消模拟模式后可使用 Qt SerialPort 连接 GRBL-like 运动控制器。
- 运动控制：支持点动步距选择、X/Y/Z 六向点动、复位、位置查询、读取版本、帮助命令和 G1 绝对坐标执行。
- 扫描区域：1 行 9 列表格配置起点、终点和 step，支持蛇形扫描和驻留时间设置。
- 仪表区域：支持 Mock Spectrum、Generic SCPI、R&S ZNA67、R&S FSW、Keysight N9020A，支持 TCP SCPI 连接、断开、查询 IDN、应用配置和单次扫描。
- 扫描流程：ScanManager 使用异步 SpectrumAcquisitionWorker 采集频谱，支持开始、暂停、继续、停止，状态栏显示坐标、时间、剩余点数、预计完成和状态。
- 数据存储：扫描开始创建任务目录，持续写入 `points.csv` 和 `traces.csv`，完成后可打开结果目录。
- 离线分析：支持加载 `traces.csv`，选择 Trace/Frequency/显示模式并生成热力图。
- 热力图增强：支持 LUT、Colorbar、自动/手动 vmin/vmax、透明度控制、主界面预览和弹窗导出 PNG。
- 真实频谱仪框架：ScanManager 扫描时优先采集已连接仪表数据，未连接时自动使用 Mock Spectrum fallback，并支持 timeout/retry/stopOnError 策略。

## Release 发布流程

仓库配置了两个 Windows workflow：

- `Windows Build`：push 到 `main` 或提交 pull request 时触发，只生成 Actions artifact，不创建 GitHub Release。
- `Release`：push `v*.*.*` tag 时触发正式发布；手动 `workflow_dispatch` 只生成测试 artifact，不创建正式 GitHub Release。

日常构建：

```powershell
git push origin main
```

构建成功后，在 Actions 中下载 artifact：

```text
NFSScanner-Windows-Release
```

正式发布：

```powershell
git tag v0.8.0
git push origin v0.8.0
```

`Release` workflow 会自动构建，并在 GitHub Releases 页面生成：

```text
NFSScanner-Windows-Portable-v0.8.0.zip
NFSScanner-Setup-v0.8.0.exe
```

两个版本区别：

- Portable：绿色版，解压即用，适合测试和现场快速验证。
- Setup：安装版，像普通 Windows 应用一样安装到 Program Files，支持开始菜单和桌面快捷方式，适合正式交付客户。

如果 tag 后没有触发：

- 检查 `.github/workflows/release.yml` 是否存在。
- 确认 `on.push.tags` 包含 `v*.*.*`。
- 如果 tag 已经推送过，建议使用新 tag，例如 `v0.3.1`。

本地构建绿色版和安装包：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_windows_msvc.ps1
powershell -ExecutionPolicy Bypass -File scripts/package_portable_windows.ps1 -Version v0.8.0
powershell -ExecutionPolicy Bypass -File scripts/build_installer_windows.ps1 -Version v0.8.0
```

本地脚本输出目录：

- `dist/NFSScanner/`：绿色版目录，安装包也会从这里取文件。
- `artifacts/NFSScanner-Windows-Portable-<Version>.zip`
- `artifacts/NFSScanner-Setup-<Version>.exe`

## 后续开发路线

1. 将真实 SCPI 设备对象迁移到设备专用线程。
2. 增加 SCPI 命令队列和更完整的异步 socket 状态机。
3. 完善扫描任务队列、断点续扫和异常恢复。
4. 增加 Trace 管理和报告导出。
5. 完成 Windows/Linux 打包、运行时依赖收集和版本签名。
