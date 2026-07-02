# NFS Scanner C++

NFS Scanner C++ 是近场扫描系统的 C++17 / Qt 6 Widgets 正式产品主线工程。当前 **v0.14.0-mock-validated** 在 v0.13.0 硬件预实现基础上增加 **全自动 Mock 验收**（无需人工点 UI）。

## 当前版本：v0.14.0-mock-validated

| 能力 | 说明 |
|------|------|
| 四页主框架 | 扫描 / 设备 / 分析 / 报告 |
| 硬件配置 | `config/hardware_config.json` + **9 个 Profile**（含 fault_injection_demo） |
| **全自动 Mock 验收** | `scripts/validation/run_full_mock_validation.ps1` → `validation_output/FULL_MOCK_VALIDATION_REPORT.md` |
| **NFSScannerCli** | `build/Release/NFSScannerCli.exe` — headless profile/bring-up/e2e/diagnostics 验证 |
| 硬件接入向导 | 设备 → **硬件接入向导** — 分步 bring-up + 报告导出 |
| 会话记录/回放 | `logs/hardware_sessions/session_*.jsonl` |
| 故障注入 | HardwareDebugDialog → Fault Injection（Mock） |
| 命令行 | `--profile` / `--safe-mode` / `--self-check` / `--export-diagnostics` |
| 硬件脚本 | `scripts/hardware/run_mock_all_check.ps1` 等 |
| 自检 | `NFSScannerSelfCheck.exe` — **163/163 PASS** |
| 真实设备 | 接口完整，**现场未验证** |

---

## 构建

### 推荐（Windows MSVC）

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_windows_msvc.ps1
```

默认 Qt 路径：`C:/Qt/6.8.3/msvc2022_64`

产物：`build/Release/NFSScanner.exe`、`build/Release/NFSScannerSelfCheck.exe`、`build/Release/NFSScannerCli.exe`

### 手动 CMake

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64"
cmake --build build --config Release
```

---

## 启动方式

> 首次使用前请先构建（见上一节）。Qt DLL 需在 PATH 中，或将 `C:/Qt/6.8.3/msvc2022_64/bin` 加入环境变量。

### 1. GUI 主程序（推荐）

**方式 A — 脚本启动（自动找 exe + 设置 Qt PATH）：**

```powershell
powershell -ExecutionPolicy Bypass -File scripts/run_windows_msvc.ps1
```

**方式 B — 直接运行 exe：**

```powershell
$env:PATH = "C:/Qt/6.8.3/msvc2022_64/bin;" + $env:PATH
cd D:\code_2026\nfs-scanner-cpp   # 切换到仓库根目录，以便读取 config/
.\build\Release\NFSScanner.exe
```

**方式 C — Portable 绿色包（无需安装 Qt 到系统 PATH）：**

```powershell
# 先打包（仅需一次）
powershell -ExecutionPolicy Bypass -File scripts/package_portable_windows.ps1 -Version v0.14.0-mock-validated

# 解压 artifacts\NFSScanner-Windows-Portable-v0.14.0-mock-validated.zip 后：
.\dist\NFSScanner\NFSScanner.exe
```

> Portable 包不含 `config/`，请将仓库内 `config/` 目录复制到 exe 同级，或在设备页手动指定配置路径。

### 2. 常用命令行参数（GUI）

在 exe 后追加参数，执行完即退出（不加参数则正常打开主窗口）：

```powershell
.\build\Release\NFSScanner.exe --help
.\build\Release\NFSScanner.exe --profile mock_all
.\build\Release\NFSScanner.exe --profile motion_only_grbl --safe-mode
.\build\Release\NFSScanner.exe --hardware-config config/hardware_config.json
.\build\Release\NFSScanner.exe --self-check
.\build\Release\NFSScanner.exe --export-diagnostics
```

| 参数 | 说明 |
|------|------|
| `--profile <name>` | 加载 `config/profiles/<name>.json` |
| `--hardware-config <path>` | 加载指定 hardware_config.json |
| `--safe-mode` | 禁止自动连接真实硬件 |
| `--self-check` | 运行 SelfCheck 后退出 |
| `--export-diagnostics` | 导出诊断包后退出 |

### 3. 自检（命令行，无 GUI）

```powershell
$env:PATH = "C:/Qt/6.8.3/msvc2022_64/bin;" + $env:PATH
.\build\Release\NFSScannerSelfCheck.exe
```

预期输出末尾：`All self-check tests passed.`（163 项）

### 4. Headless 验收 CLI（无 GUI）

```powershell
$env:PATH = "C:/Qt/6.8.3/msvc2022_64/bin;" + $env:PATH
.\build\Release\NFSScannerCli.exe --help

# 校验全部 Profile
.\build\Release\NFSScannerCli.exe --validate-profiles --profile-dir config/profiles --output validation_output

# Mock 全流程验收（一键，见下一节脚本）
powershell -ExecutionPolicy Bypass -File scripts/validation/run_full_mock_validation.ps1
```

### 5. 首次启动建议流程（Mock，不接硬件）

1. 构建：`scripts/build_windows_msvc.ps1`
2. 自检：`NFSScannerSelfCheck.exe`
3. 启动 GUI：`NFSScanner.exe --profile mock_all`
4. 菜单 **设备 → 硬件接入向导** 跑 Mock bring-up
5. 扫描页开始 Mock 扫描 → 分析页加载 `traces.csv`

默认 **Mock 模式**：无真实串口/仪表亦可演示扫描与分析。

---

## SelfCheck

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_windows_msvc.ps1
$env:PATH = "C:/Qt/6.8.3/msvc2022_64/bin;" + $env:PATH
.\build\Release\NFSScannerSelfCheck.exe
```

预期：`All self-check tests passed.`（163 项，含 bring-up 向导、session 回放、故障注入、error_policy、CLI）

---

## 全自动 Mock 验收（无需人工点 UI）

```powershell
powershell -ExecutionPolicy Bypass -File scripts/validation/run_full_mock_validation.ps1
```

或分步：

```powershell
.\build\Release\NFSScannerCli.exe --profile mock_all --profile-dir config/profiles --run-full-validation --output validation_output
.\build\Release\NFSScannerCli.exe --generate-validation-report --input validation_output --output validation_output
```

报告：`validation_output/FULL_MOCK_VALIDATION_REPORT.md`  
说明：[scripts/validation/README.md](scripts/validation/README.md)

---

## 真实硬件预实现状态（v0.14.0-mock-validated）

### 支持设备列表

| 子系统 | Mock | 真实接口 | 当前验证状态 |
|--------|------|----------|--------------|
| 运动平台 | MockMotionController | GRBL 串口（SerialMotionController） | Mock 已验证；GRBL 待现场 |
| 频谱仪 | MockSpectrumAnalyzer | ZNA67 / FSW / N9020A / Generic SCPI | Mock 已验证；SCPI 待现场 |
| 相机 | MockCamera | USB Stub / Industrial Stub | Mock 已验证；Stub 待 SDK |
| 探头 Hx/Hy | MockProbeController | Serial Stub / SDK Stub | Mock 已验证；Stub 待现场 |

### Profile 快速切换

`config/profiles/` 提供 8 个预设：`mock_all`、`motion_only_grbl`、`spectrum_only_*`、`grbl_*_default`。  
设备页 → Profile 下拉框 → **加载 / 保存 / 校验 / 打开配置目录**。  
详见 [docs/hardware/HARDWARE_CONFIG_PROFILES.md](docs/hardware/HARDWARE_CONFIG_PROFILES.md)

### 不接硬件时能做什么

- Mock 全流程扫描与分析
- 运行 `NFSScannerSelfCheck.exe`（163 项）
- 运行 **全自动 Mock 验收**（`scripts/validation/run_full_mock_validation.ps1`）
- 使用 Python 模拟器验证 GRBL/SCPI TCP 链路（`tools/simulators/`）
- 设备页单项测试、Bring-up Test（Mock）、导出 Bring-up 报告
- 导出诊断包（Help → 导出诊断包）

### 推荐现场调试顺序

1. `mock_all` — 软件演示基线  
2. `motion_only_grbl` — 单独调运动  
3. `spectrum_only_zna67` / `fsw` / `n9020a` — 单独调频谱仪  
4. `grbl_*_default` — 运动 + 对应仪表  
5. 相机 → 探头 → 完整扫描  

详见 [docs/hardware/REAL_HARDWARE_BRINGUP_GUIDE.md](docs/hardware/REAL_HARDWARE_BRINGUP_GUIDE.md)

### 关键入口

- **hardware_config.json** — 主配置  
- **DevicePage** — Profile、单项测试、Bring-up、SCPI 日志  
- **HardwareDebugDialog** — Help → 硬件调试面板  
- **PreScanChecklist** — 扫描前自动检查（扫描页显示 hardware_mode）  
- **Diagnostics package** — [docs/hardware/DIAGNOSTIC_PACKAGE_GUIDE.md](docs/hardware/DIAGNOSTIC_PACKAGE_GUIDE.md)

---

## 真实硬件支持（配置说明）

### 配置文件

路径：`config/hardware_config.json`（首次运行可自动生成）

```json
{
  "motion": { "enabled": false, "type": "grbl", "port": "COM3", "baudrate": 115200 },
  "spectrum": { "enabled": false, "type": "mock", "address": "192.168.0.10", "port": 5025 },
  "camera": { "enabled": false, "type": "mock", "save_dir": "images" },
  "probe": { "enabled": false, "type": "mock", "orientation": "Hx" }
}
```

### Mock / Real 切换

- **运动**：设备页「模拟模式」勾选 = Mock；取消 + 串口连接 = Real
- **频谱**：`spectrum.type=mock` 或设备页 Mock Spectrum；Real 需 TCP 5025 连接
- **相机/探头**：默认 Mock；USB/工业相机与 Serial/SDK 探头为 Stub

### 设备诊断

菜单 **Help → 诊断信息**，导出至 `logs/diagnostics_YYYYMMDD_HHMMSS.md`

### 扫描前 Checklist

点击「开始扫描」后自动检查：项目、限位、设备连接、输出目录、Alignment（warning）、License

### 现场联调

详见 [docs/hardware/REAL_HARDWARE_BRINGUP_GUIDE.md](docs/hardware/REAL_HARDWARE_BRINGUP_GUIDE.md)

### 当前未接硬件限制

- 启动时不自动连接设备；不写死 IP/COM
- USB 相机 / 工业相机 / 真实探头控制待 SDK 或现场确认
- 危险运动命令需用户显式点击

---

## Portable 绿色版打包

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_windows_msvc.ps1
powershell -ExecutionPolicy Bypass -File scripts/package_portable_windows.ps1 -Version v0.14.0-mock-validated
```

输出：

- `dist/NFSScanner/` — 含 `NFSScanner.exe`、Qt6 DLL、`platforms/`、`styles/`、`resources/`
- `artifacts/NFSScanner-Windows-Portable-v0.14.0-mock-validated.zip`

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
# 可选 tag：
git tag v0.14.0-mock-validated
git push origin v0.14.0-mock-validated
```

GitHub Actions：`Windows Build`（PR/push）、`Release`（tag `v*.*.*`）。

本地输出目录：

- `dist/NFSScanner/`
- `artifacts/NFSScanner-Windows-Portable-<Version>.zip`
- `artifacts/NFSScanner-Setup-<Version>.exe`
