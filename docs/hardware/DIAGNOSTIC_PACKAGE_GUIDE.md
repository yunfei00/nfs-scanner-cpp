# 诊断包导出指南

v0.12.0 提供两级诊断导出，用于现场问题复现、硬件联调记录和版本追踪。

## 导出方式对比

| 方式 | 入口 | 输出路径 | 内容 |
|------|------|----------|------|
| **Markdown 摘要** | Help → 诊断信息 | `logs/diagnostics_YYYYMMDD_HHMMSS.md` | 配置摘要、设备状态、许可、最近日志 |
| **完整诊断包** | `DiagnosticPackageExporter::exportPackage()` | `logs/diagnostics/NFSScanner_Diagnostics_YYYYMMDD_HHMMSS/` | 上述 + JSON 快照 + 最近 log 文件 |

> UI 菜单当前默认导出 Markdown 摘要。完整诊断包可通过后续 UI 按钮或集成测试调用 `exportPackage`。

## Markdown 摘要（Help → 诊断信息）

### 操作步骤

1. 启动 NFSScanner，完成当前联调操作（连接设备、失败复现等）。  
2. 菜单 **Help → 诊断信息**。  
3. 弹出框显示导出路径；同时写入主窗口日志区。  
4. 打开 `logs/diagnostics_*.md` 查看。

### 文件内容结构

```markdown
# NFSScanner Hardware Diagnostics

- generated_at: ...
- app: NFSScanner v0.12.0
- qt: ...
- data_format: ...

## Project
## Hardware Config Summary
## Device States
## License
## SelfCheck
## Recent Logs (max N)
```

### 典型用途

- 快速记录某次连接失败时的设备状态  
- 附在问题报告邮件中（体积小）  
- 对比联调前后 `Device States` 变化  

---

## 完整诊断包（DiagnosticPackageExporter）

### 目录结构

```
logs/diagnostics/NFSScanner_Diagnostics_20260702_143022/
├── diagnostics.md                 # 与 Markdown 摘要相同
├── hardware_config_snapshot.json  # 当前 hardware_config 完整 JSON
├── device_status_snapshot.json    # motion/spectrum/camera/probe 连接状态
├── self_check_summary.txt         # SelfCheck 结果摘要（若已传入）
└── latest_logs/                   # 最近最多 20 个 *.log 副本
    ├── app_20260702.log
    └── ...
```

### 编程调用示例

```cpp
NFSScanner::Diagnostics::DiagnosticPackageOptions options;
options.deviceManager = deviceManager;
options.licenseManager = licenseManager;
options.projectManager = projectManager;
options.selfCheckSummary = QStringLiteral("NFSScannerSelfCheck: 42/42 PASS");

QString outputDir;
if (NFSScanner::Diagnostics::DiagnosticPackageExporter::exportPackage(&options, &outputDir)) {
    // outputDir = logs/diagnostics/NFSScanner_Diagnostics_...
}
```

### SelfCheck 配合

联调前/后各运行一次：

```powershell
build\Release\NFSScannerSelfCheck.exe
```

将控制台输出摘要填入 `selfCheckSummary`，或手动写入 `self_check_summary.txt`。

---

## 推荐导出时机

| 阶段 | 建议 |
|------|------|
| 首次安装 | SelfCheck PASS 后立即导出 |
| 单硬件验证通过 | 连接真机/模拟器后导出 |
| 扫描失败 | **失败当下**导出（保留 last_error 与日志） |
| 版本升级对比 | 升级前后各一份，对比 `hardware_config_snapshot.json` |
| 交付客户 | 打包整个 `NFSScanner_Diagnostics_*` 目录为 zip |

## 现场打包步骤（Windows）

1. 复现问题（或确认 PASS）。  
2. Help → 诊断信息（生成 `diagnostics_*.md`）。  
3. 若已集成完整包导出，执行导出；否则手动复制：  
   - `logs/diagnostics_*.md`  
   - `config/hardware_config.json`  
   - `logs/*.log`（最近几份）  
   - 失败任务的 `project/scans/<task>/scan_config.json`、`points.csv` 片段  
4. 压缩为 `NFSScanner_support_YYYYMMDD.zip`，附简要说明（操作步骤、COM/IP、仪表型号）。

## 隐私与脱敏

诊断包可能包含：

- 项目路径、机器 ID  
- 硬件 IP/COM 配置  
- 应用日志（无完整 trace 大数据）

**不会**包含：完整 `traces.csv`、许可证私钥、频谱原始大数组。

交付外部前请检查 `hardware_config_snapshot.json` 与日志中是否有内部 IP/路径，必要时手工脱敏。

## 路径说明

所有路径相对于可执行文件：

| 资源 | 相对路径 |
|------|----------|
| 日志根目录 | `../logs` |
| 硬件配置 | `../config/hardware_config.json` |
| 扫描任务 | `../project/scans/<task>/` |

## 故障排查

| 现象 | 处理 |
|------|------|
| 导出失败 | 确认 `logs/` 目录可写；检查磁盘空间 |
| 设备状态全 Disconnected | 先连接设备再导出 |
| `latest_logs/` 为空 | 确认 Logger 已写入 `*.log` |
| 配置与 UI 不一致 | 设备页保存配置后重新导出 |

## 相关文档

- [REAL_HARDWARE_BRINGUP_GUIDE.md](REAL_HARDWARE_BRINGUP_GUIDE.md)  
- [HARDWARE_CONFIG_PROFILES.md](HARDWARE_CONFIG_PROFILES.md)  
- [SELF_TEST_CHECKLIST.md](../migration/SELF_TEST_CHECKLIST.md)  
