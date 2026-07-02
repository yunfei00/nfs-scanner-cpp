# 真实硬件现场联调总指南

## 适用范围

本文档适用于 **v0.12.0-hardware-ready** 及之后版本。软件层已预实现 Mock / 真实双路径，**未接硬件时** 仍可启动、Mock 扫描、SelfCheck。

v0.12.0 新增：

- `config/profiles/` 硬件配置模板 — 见 [HARDWARE_CONFIG_PROFILES.md](HARDWARE_CONFIG_PROFILES.md)
- `tools/simulators/` GRBL / SCPI Python 模拟器 — 见 [tools/simulators/README.md](../../tools/simulators/README.md)
- 完整诊断包导出 — 见 [DIAGNOSTIC_PACKAGE_GUIDE.md](DIAGNOSTIC_PACKAGE_GUIDE.md)
- 分型号 Bring-up：[ZNA67_BRINGUP.md](ZNA67_BRINGUP.md)、[FSW_BRINGUP.md](FSW_BRINGUP.md)、[N9020A_BRINGUP.md](N9020A_BRINGUP.md)

## 接线前检查

- [ ] 运动控制器、频谱仪、相机、探头电源与接地符合设备手册
- [ ] 串口/USB/网线连接牢固，避免与高压探头线缆捆扎
- [ ] 确认 GRBL 控制器已解锁且急停可用
- [ ] 确认频谱仪 IP 与 PC 同网段
- [ ] **不要在无人看管时发送 Home 或大行程移动**

## 配置文件

默认路径（相对可执行文件）：

```text
../config/hardware_config.json
```

首次启动若文件不存在，程序会自动生成默认配置（全部 `enabled: false`，Mock 友好）。

也可从 Profile 复制：

```powershell
copy config\profiles\mock_all.json config\hardware_config.json
```

## Mock / Real 切换

| 子系统 | Mock 条件 | Real 条件 |
|--------|-----------|-----------|
| 运动 | 设备页勾选「模拟模式」 | 取消模拟 + `motion.enabled=true` + 串口连接 |
| 频谱 | `spectrum.type=mock` 或未选真实仪表 | `spectrum.enabled=true` + TCP 连接成功 |
| 相机 | `camera.type=mock` | USB/工业 SDK 待接入；Mock 可拍照 |
| 探头 | `probe.type=mock` | Serial/SDK Stub 待现场确认 |

## 推荐联调顺序

按风险从低到高、依赖从少到多逐项验证：

| 步骤 | Profile / 配置 | 验证目标 | 模拟器 / 备注 |
|------|----------------|----------|---------------|
| **1** | `mock_all` | 软件启动、SelfCheck、Mock 扫描、UI 全流程 | 无需硬件 |
| **2** | `motion_only_grbl` | GRBL 连接、`$I`/`?`/G1/限位/Home | `grbl_motion_simulator.py` + com0com/com2tcp |
| **3** | `spectrum_only_zna67`（或 fsw / n9020a） | SCPI 连接、IDN、configure、单次 trace | `scpi_spectrum_simulator.py --device <型号>` |
| **4** | `grbl_zna67_default`（或 `grbl_fsw_default` / `grbl_n9020a_default`） | 运动 + 频谱联合，**单点扫描** | 同时启动两个模拟器或接真机 |
| **5** | 在步骤 4 基础上启用 camera | 拍照、Alignment 背景（Stub/Mock） | `camera.enabled=true` |
| **6** | 启用 probe | Hx/Hy 切换、`probe_orientation` | `probe.enabled=true` |
| **7** | **full scan** | 小区域网格扫描、`traces.csv` / `points.csv` 完整 | 全真实或混合 Mock |

### 各步骤要点

1. **mock_all** — 运行 `NFSScannerSelfCheck.exe`，确认 42/42 PASS（或当前版本全部 PASS）。
2. **motion_only_grbl** — 修改 `motion.port` 为 com0com 对之一；见 [GRBL_MOTION_CHECKLIST.md](GRBL_MOTION_CHECKLIST.md)。
3. **spectrum_only_*** — 按仪表阅读对应 Bring-up 文档；`address` 设为 `127.0.0.1` 可对接模拟器。
4. **grbl_*_default** — 扫描前 Checklist 运动+频谱均通过；首点核对 MPos 与 trace 行。
5. **camera** — 当前 USB/工业相机为 Stub；Mock 可验证流程。
6. **probe** — Serial/SDK Stub；Mock 验证 Hx/Hy 与 `switch_delay_ms`。
7. **full scan** — 2×2 或 3×3 小网格；导出诊断包备查。

## 日志与诊断

- UI 日志区：主窗口底部
- Markdown 诊断：**Help → 诊断信息** → `logs/diagnostics_YYYYMMDD_HHMMSS.md`
- 完整诊断包：`logs/diagnostics/NFSScanner_Diagnostics_*/` — 见 [DIAGNOSTIC_PACKAGE_GUIDE.md](DIAGNOSTIC_PACKAGE_GUIDE.md)
- 扫描任务：`project/scans/<task>/points.csv`、`traces.csv`、`scan_config.json`

## 失败回退 Mock

1. 设备页勾选「模拟模式」（运动）
2. `hardware_config.json` 中设置 `spectrum.type: mock`
3. 加载 `mock_all` profile
4. 断开全部设备（设备页单项测试区）
5. 重新运行 SelfCheck 确认软件正常

## 需人工记录的结果

| 项目 | 记录内容 |
|------|----------|
| 串口 | COM 口、波特率、$I/?/$H 响应 |
| 频谱仪 | *IDN?、单次 sweep、trace 行数 |
| 相机 | 拍照路径、Alignment 背景是否正常 |
| 探头 | Hx/Hy 切换延时、scan_config 中 probe_orientation |
| 扫描 | 首点坐标、首行 trace、失败重试次数 |
| Profile | 当前使用的 profile 名称 |
| 诊断包 | 导出路径与时间戳 |

## 子文档

| 文档 | 内容 |
|------|------|
| [HARDWARE_CONFIG_PROFILES.md](HARDWARE_CONFIG_PROFILES.md) | Profile 列表与加载 |
| [DIAGNOSTIC_PACKAGE_GUIDE.md](DIAGNOSTIC_PACKAGE_GUIDE.md) | 诊断导出 |
| [GRBL_MOTION_CHECKLIST.md](GRBL_MOTION_CHECKLIST.md) | GRBL 人工清单 |
| [SCPI_SPECTRUM_CHECKLIST.md](SCPI_SPECTRUM_CHECKLIST.md) | SCPI 人工清单 |
| [ZNA67_BRINGUP.md](ZNA67_BRINGUP.md) | ZNA67 专项 |
| [FSW_BRINGUP.md](FSW_BRINGUP.md) | FSW 专项 |
| [N9020A_BRINGUP.md](N9020A_BRINGUP.md) | N9020A 专项 |
| [CAMERA_CHECKLIST.md](CAMERA_CHECKLIST.md) | 相机 |
| [PROBE_HX_HY_CHECKLIST.md](PROBE_HX_HY_CHECKLIST.md) | 探头 |
| [tools/simulators/README.md](../../tools/simulators/README.md) | 模拟器用法 |

## 当前限制（未接硬件）

- USB / 工业相机为 Stub，不引入 OpenCV
- 探头 Serial/SDK 为 Stub，Hx/Hy 仅 Mock 可切换
- 真实 GRBL / SCPI 需现场 IP/COM 配置，软件不写死
- 启动时 **不会** 自动连接任何真实设备
- GRBL 模拟器为 TCP，Windows 需 com0com + com2tcp 桥接至串口
