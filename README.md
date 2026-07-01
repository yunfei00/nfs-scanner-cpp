# NFS Scanner C++

NFS Scanner C++ 是近场扫描系统的 C++17 / Qt 6 Widgets 正式产品主线工程。当前 **v0.10.0-alpha** 提供四页产品主框架、真实运动/频谱扫描、项目文件夹、离线分析、报告导出、Ed25519 授权校验与完整 Mock fallback。

## 当前版本：v0.10.0-alpha

| 能力 | 说明 |
|------|------|
| 四页主框架 | 扫描 / 设备 / 分析 / 报告（Project 仅文件菜单，非一级导航） |
| 页面类 | `ScanPage`、`DevicePage`、`AnalysisPage`、`ReportPage` |
| 设备管理 | `DeviceManager` + `DeviceStatusBar` 六芯片状态 |
| 项目文件夹 | `ProjectManager`：`project.json`、`scans/`、`reports/`、`workspace/` 回退 |
| Alignment | 线性矩形 + **四点透视**（`QTransform::quadToQuad`，无 OpenCV） |
| 报告 | HTML / Markdown / PDF / PNG 集合 |
| 授权 | `LicenseManager` + Ed25519 签名校验（见 [LICENSE_SIGNING.md](docs/migration/LICENSE_SIGNING.md)） |
| 自检 | `NFSScannerSelfCheck.exe` — **47/47 PASS** |
| 真实设备 | ZNA67 / FSW / N9020A SCPI + GRBL 串口运动（代码就绪，硬件需人工验证） |

---

## 构建

### 推荐（Windows MSVC）

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_windows_msvc.ps1
```

默认 Qt 路径：`C:/Qt/6.8.3/msvc2022_64`

产物：`build/Release/NFSScanner.exe`、`build/Release/NFSScannerSelfCheck.exe`

### 手动 CMake

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build --config Release
```

---

## 运行

```powershell
powershell -ExecutionPolicy Bypass -File scripts/run_windows_msvc.ps1
# 或
$env:PATH = "C:/Qt/6.8.3/msvc2022_64/bin;" + $env:PATH
.\build\Release\NFSScanner.exe
```

默认 **Mock 模式**：无真实串口/仪表亦可演示扫描与分析。

---

## SelfCheck

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_windows_msvc.ps1
$env:PATH = "C:/Qt/6.8.3/msvc2022_64/bin;" + $env:PATH
.\build\Release\NFSScannerSelfCheck.exe
```

预期：`All self-check tests passed.`（47 项，含透视标定、traces.csv 解析、Ed25519 签名、项目路径、报告导出）

---

## Portable 绿色版打包

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_windows_msvc.ps1
powershell -ExecutionPolicy Bypass -File scripts/package_portable_windows.ps1 -Version v0.10.0-alpha
```

输出：

- `dist/NFSScanner/` — 含 `NFSScanner.exe`、Qt6 DLL、`platforms/`、`styles/`、`resources/`
- `artifacts/NFSScanner-Windows-Portable-v0.10.0-alpha.zip`

---

## Installer 安装包

**需先安装 [Inno Setup 6](https://jrsoftware.org/isinfo.php)。**  
ISCC 典型路径：`%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe`（2026-07-02 本地验证通过）

```powershell
$env:PATH = "$env:LOCALAPPDATA\Programs\Inno Setup 6;" + $env:PATH
powershell -ExecutionPolicy Bypass -File scripts/build_installer_windows.ps1 -Version v0.10.0-alpha
```

输出：`artifacts/NFSScanner-Setup-v0.10.0-alpha.exe`

---

## 项目文件夹结构

```text
ProjectName/
  project.json
  scans/
    scan_YYYYMMDD_HHMMSS/
      meta.json
      scan_config.json
      points.csv
      traces.csv
      alignment.json   # 可选
  images/
  reports/
  logs/
  exports/
```

无打开项目时使用：

```text
%LOCALAPPDATA%/NFSScanner/workspace/
  scans/
  reports/
```

扫描默认写入 `project/scans/` 或 `workspace/scans/`；报告默认写入 `project/reports/` 或 `workspace/reports/`。

---

## Alignment 说明

| 模式 | JSON `mapping_mode` | 说明 |
|------|---------------------|------|
| 线性矩形 | `linear_rectangle` | 世界/像素 min-max 线性映射 |
| 四点透视 | `perspective_four_point` | 四角控制点 + `QTransform::quadToQuad` |

- 扫描 Dock 内 `AlignmentEditor` 可切换模式、加载背景图、Mock 相机截图、保存/加载 `alignment.json`
- **无 alignment.json 时扫描与分析仍可正常运行**

---

## License 说明

- 无 `license.json` → **Demo 模式**（允许 scan / analysis / report）
- 有 `license.json` 且 `signature` 为空 → machine_id 绑定 Demo 规则
- 有 `signature` + `signature_alg=ed25519` → 必须通过 Ed25519 校验

签发流程与厂商私钥保管见 [docs/migration/LICENSE_SIGNING.md](docs/migration/LICENSE_SIGNING.md)（**私钥不在仓库**）。

---

## 真实硬件验证边界

以下项 **代码已接入，需现场人工验证**：

| 项 | 说明 |
|----|------|
| GRBL 串口运动平台 | COM 口 + 真实平台 |
| ZNA67 / FSW / N9020A | TCP SCPI + 仪表 IP |
| USB 工业相机 | 驱动未引入 |
| 舵机 Hx/Hy | 未实现 |
| GUI 完整扫描流程 | 四页切换、Mock 扫描、报告 PDF 导出 |
| 正式 license 签发 | 厂商 Ed25519 私钥 + 签发工具 |

完整清单：`docs/migration/SELF_TEST_CHECKLIST.md`

---

## 已知限制

- 四点透视为四边形映射，**非 OpenCV 多点标定**；矩形拖拽/GUI 悬停读数需手测
- USB 相机 / OpenCV 未引入
- Installer 需 Inno Setup 6；ISCC 位于 `%LOCALAPPDATA%\Programs\Inno Setup 6\`（2026-07-02 本地验证通过）
- 厂商生产 license 私钥与签发工具需人工部署
- SCPI 连接/配置在设备专用线程，采集在 `SpectrumAcquisitionWorker`（UI 不阻塞等待网络 SCPI）

---

## 依赖

- C++17、CMake 3.20+
- Qt 6.8.3（Core、Gui、Widgets、SerialPort、Network、PrintSupport）
- Windows：MSVC 2022 或 MinGW-w64
- 可选：Inno Setup 6（安装包）

---

## 迁移文档

- [MIGRATION_FROM_NFS_SCANNER_PRO.md](docs/migration/MIGRATION_FROM_NFS_SCANNER_PRO.md)
- [IMPLEMENTATION_BACKLOG.md](docs/migration/IMPLEMENTATION_BACKLOG.md)
- [SELF_TEST_CHECKLIST.md](docs/migration/SELF_TEST_CHECKLIST.md)
- [LICENSE_SIGNING.md](docs/migration/LICENSE_SIGNING.md)

---

## Release / CI

```powershell
git push origin feature/full-python-pro-migration
# 正式发布 tag（需人工 push）：
git tag v0.10.0-alpha
git push origin v0.10.0-alpha
```

GitHub Actions：`Windows Build`（PR/push）、`Release`（tag `v*.*.*`）。

本地输出目录：

- `dist/NFSScanner/`
- `artifacts/NFSScanner-Windows-Portable-<Version>.zip`
- `artifacts/NFSScanner-Setup-<Version>.exe`
