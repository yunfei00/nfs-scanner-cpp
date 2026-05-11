# NFS Scanner C++

NFS Scanner C++ 是近场扫描系统商业版的 C++/Qt6 重构工程。当前阶段聚焦于可运行的企业级 UI 骨架、清晰的分层结构，以及不阻塞界面的 Mock 扫描流程，为后续接入运动控制、频谱仪、相机、热力图、授权和打包发布打基础。

## 项目目标

- 使用 C++17 和 Qt 6 Widgets 重构近场扫描系统客户端。
- 以 CMake 作为跨平台构建入口，支持 Windows 和 Linux。
- 首阶段提供可运行的 UI 框架、Mock 设备层、Mock 扫描流程和热力图占位视图。
- 后续逐步接入真实硬件、数据分析、商业授权和安装包流程。

## 依赖

- C++17 兼容编译器
  - Windows: MSVC 2022 或 MinGW-w64
  - Linux: GCC 10+ 或 Clang 12+
- Qt 6，至少包含 Core、Gui、Widgets 模块
- CMake 3.20+

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

默认会通过 `aqtinstall` 安装 Qt `6.8.3` 的 `win64_msvc2022_64` 包到 `C:/Qt`，构建脚本使用的 Qt 路径为 `C:/Qt/6.8.3/msvc2022_64`。如果 Qt 已安装，安装脚本会直接复用现有目录。

## 本地构建

### Windows 方式一：Visual Studio 2022 + Qt MSVC

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

### Windows 方式二：MinGW + Qt MinGW

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

如果 Qt 已通过系统包管理器安装并且 CMake 可以自动找到 Qt6，可以省略 `CMAKE_PREFIX_PATH`。

## 当前功能

- Qt Widgets 主窗口骨架。
- 顶部菜单栏、左右控制区、中部热力图视图、底部日志区、状态栏。
- Mock 运动控制器和 Mock 频谱仪接口。
- `ScanWorker` 使用 `QTimer` 每 100ms 推进一个扫描点，不阻塞 UI。
- 热力图视图绘制模拟相机区域、网格背景、渐变热力图和缩放文字。

## GitHub Actions 自动构建

仓库已配置 Windows 自动构建 workflow：`Windows Build`。

- 每次 push 到 `main` 分支会自动构建 Windows Release 版本。
- 每次 pull request 到 `main` 分支也会执行构建检查。
- 也可以在 GitHub 仓库的 Actions 页面手动触发 `Windows Build`。
- 构建成功后，在 workflow run 的 artifacts 中下载 `NFSScanner-Windows-Release`。
- artifact 解压后可以直接运行 `NFSScanner.exe`。
- 后续正式版本发布会使用 tag + Release workflow。

## 后续开发路线

1. 设备接入：实现真实运动控制器、频谱仪、相机 SDK 适配层。
2. 扫描编排：加入扫描任务队列、暂停/恢复、异常恢复和断点续扫。
3. 数据分析：接入真实热力图生成、插值、LUT、Trace 管理和结果导出。
4. 工程配置：加入项目文件、扫描模板、设备 profile 和用户偏好。
5. 商业能力：加入授权校验、日志审计、崩溃诊断和自动更新。
6. 发布打包：完善 Windows/Linux 安装包、运行时依赖收集和版本签名。
