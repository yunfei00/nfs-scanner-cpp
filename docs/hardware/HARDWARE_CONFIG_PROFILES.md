# 硬件配置 Profile 说明

v0.12.0 起，`config/profiles/` 提供预置硬件配置模板，用于分阶段联调。Profile 为完整 `hardware_config.json` 快照，可通过代码或手动复制加载。

## Profile 目录

```
config/profiles/
├── mock_all.json              # 全部 Mock，软件自检
├── motion_only_grbl.json      # 仅真实 GRBL 运动
├── spectrum_only_zna67.json   # 仅 ZNA67 频谱
├── spectrum_only_fsw.json     # 仅 FSW 频谱
├── spectrum_only_n9020a.json# 仅 N9020A 频谱
├── grbl_zna67_default.json    # GRBL + ZNA67 默认组合
├── grbl_fsw_default.json      # GRBL + FSW
└── grbl_n9020a_default.json   # GRBL + N9020A
```

## Profile 一览

| Profile | motion | spectrum | camera | probe | 用途 |
|---------|--------|----------|--------|-------|------|
| `mock_all` | Mock | mock | mock | mock | 无硬件开发/演示 |
| `motion_only_grbl` | **Real GRBL** | mock | mock | mock | 运动平台单测 |
| `spectrum_only_zna67` | Mock | **zna67** | mock | mock | ZNA67 单测 |
| `spectrum_only_fsw` | Mock | **fsw** | mock | mock | FSW 单测 |
| `spectrum_only_n9020a` | Mock | **n9020a** | mock | mock | N9020A 单测 |
| `grbl_zna67_default` | **Real** | **zna67** | mock | mock | 扫描联调（ZNA） |
| `grbl_fsw_default` | **Real** | **fsw** | mock | mock | 扫描联调（FSW） |
| `grbl_n9020a_default` | **Real** | **n9020a** | mock | mock | 扫描联调（N9020A） |

> **Real GRBL**：`motion.enabled=true` 且设备页取消「模拟模式」。  
> **Mock 运动**：设备页勾选「模拟模式」，或 `motion.enabled=false`。

## 加载方式

### 方式 1：手动复制（最通用）

```powershell
copy config\profiles\grbl_zna67_default.json config\hardware_config.json
```

路径相对可执行文件为 `../config/hardware_config.json`。修改后重启 NFSScanner 或在设备页重新加载。

### 方式 2：API / DeviceManager

```cpp
deviceManager->loadHardwareProfile("grbl_zna67_default");
```

内部调用 `HardwareConfigManager::loadProfile()`，从 `config/profiles/<name>.json` 读取。

### 方式 3：列出可用 Profile

```cpp
HardwareConfigManager manager;
QStringList names = manager.listProfiles();
// mock_all, motion_only_grbl, spectrum_only_zna67, ...
```

## 加载前必改字段

Profile 中的占位值需按现场修改：

| 字段 | 典型值 | 说明 |
|------|--------|------|
| `motion.port` | `COM3` / `COM11` | GRBL 串口（com0com 对之一） |
| `motion.baudrate` | `115200` | 与控制器一致 |
| `spectrum.address` | `127.0.0.1` 或仪表 IP | 模拟器用 127.0.0.1 |
| `spectrum.port` | `5025` | SCPI TCP |
| `spectrum.start_freq_hz` / `stop_freq_hz` | 按测试计划 | 单位 Hz |
| `camera.device_index` | `0` | USB 相机索引（Stub 阶段可忽略） |
| `probe.orientation` | `Hx` / `Hy` | 探头极化 |

## 推荐联调顺序

与 [REAL_HARDWARE_BRINGUP_GUIDE.md](REAL_HARDWARE_BRINGUP_GUIDE.md) 一致：

1. **`mock_all`** — 确认软件、SelfCheck、Mock 扫描正常  
2. **`motion_only_grbl`** — GRBL + 模拟器/com2tcp，验证 `$I`/`?`/G1/限位  
3. **`spectrum_only_*`** — 按仪表型号选 profile + SCPI 模拟器或真机  
4. **`grbl_*_default`** — 运动 + 频谱联合，单点扫描  
5. **camera** — 在 profile 中设 `camera.enabled=true`（当前 Stub/Mock）  
6. **probe** — 设 `probe.enabled=true`，验证 Hx/Hy  
7. **full scan** — 小区域网格扫描 + traces.csv 校验  

## 自定义 Profile

1. 在设备页配置好各项，保存为 `hardware_config.json`。  
2. 复制到 `config/profiles/my_site.json`。  
3. 或通过 `HardwareConfigManager::saveProfile("my_site")` 写入。

`HardwareConfigManager::validateProfile()` 会校验 JSON 字段；camera/probe 的 usb/serial 类型会给出 Stub 警告。

## 与模拟器配合

| Profile | 需启动的模拟器 |
|---------|----------------|
| `motion_only_grbl` | `grbl_motion_simulator.py` + com2tcp |
| `spectrum_only_zna67` | `scpi_spectrum_simulator.py --device zna67` |
| `grbl_fsw_default` | GRBL + `scpi_spectrum_simulator.py --device fsw` |

详见 [tools/simulators/README.md](../../tools/simulators/README.md)。

## 相关文档

- [REAL_HARDWARE_BRINGUP_GUIDE.md](REAL_HARDWARE_BRINGUP_GUIDE.md) — 总联调流程  
- [DIAGNOSTIC_PACKAGE_GUIDE.md](DIAGNOSTIC_PACKAGE_GUIDE.md) — 诊断包导出  
- [GRBL_MOTION_CHECKLIST.md](GRBL_MOTION_CHECKLIST.md)  
- [SCPI_SPECTRUM_CHECKLIST.md](SCPI_SPECTRUM_CHECKLIST.md)  
