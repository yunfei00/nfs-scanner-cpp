# Hardware Ready Gap Report

> 版本：v0.13.0-hardware-final  
> 分支：`feature/full-python-pro-migration`  
> 日期：2026-07-02

## 已完成的硬件预实现能力

| 类别 | 能力 | 状态 |
|------|------|------|
| 配置 | 8+ Profile、`hardware_config.json`、SCPI JSON | ✅ |
| 运动 | GRBL 命令构建/解析、SerialMotionController 状态机 | ✅ 软件层 |
| 频谱 | SCPI Profile/Logger、ZNA67/FSW/N9020A 类 | ✅ 软件层 |
| 相机/探头 | Mock + USB/Industrial/Serial/SDK Stub | ✅ |
| 向导 | HardwareBringupWizard + HardwareBringupRunner | ✅ Mock 全流程 |
| 调试 | HardwareDebugDialog、故障注入面板 | ✅ |
| 会话 | HardwareSessionRecorder/Replay (JSONL) | ✅ |
| 诊断 | DiagnosticPackageExporter、DeviceStatusSnapshot | ✅ |
| 扫描 | hardware_mode、error_policy、failed_points、timing | ✅ |
| CLI | `--profile`、`--safe-mode`、`--self-check`、`--export-diagnostics` | ✅ |
| 脚本 | `scripts/hardware/*.ps1` | ✅ |
| 模拟器 | GRBL TCP + SCPI TCP Python 工具 | ✅ |
| 自检 | NFSScannerSelfCheck **163/163 PASS** | ✅ |

## 仍缺的软件能力（本阶段已补齐项标记 ✅）

| 项 | 说明 | 状态 |
|----|------|------|
| Bring-up 向导 | 分步 Profile/运动/频谱/相机/探头/Checklist/报告 | ✅ v0.13 |
| Session 回放 | JSONL 记录 + parser 回放 | ✅ v0.13 |
| 故障注入 | Mock 设备 + fault_injection_demo profile | ✅ v0.13 |
| ScanErrorPolicy 五策略 | ManualConfirm、MockFallbackExplicit | ✅ v0.13 |
| 命令行参数 | profile/safe-mode/self-check/diagnostics | ✅ v0.13 |
| Portable 内置 config | 绿色包需同目录拷贝 config/ | ⚠️ 文档说明 |
| OpenCV/厂商 SDK | USB/工业相机真实采集 | ❌ 待 SDK |
| 真实 GRBL 长时稳定性 | 连续扫描 1000+ 点 | ❌ 待现场 |

## 只能等真实硬件验证的能力

- GRBL 串口 Home/限位/Y 负方向现场坐标
- ZNA67/FSW/N9020A 真实 trace 格式与解析精度
- 探头 Hx/Hy 继电器/串口时序
- 真实扫描 SNR/重复性
- Installer 在目标机型的完整安装验证

## 可通过 Mock / Simulator 自动验证的能力

- Profile 加载/校验
- PreScanChecklist 四种 hardware_mode
- Mock bring-up 全流程
- Session 记录与回放
- 故障注入（超时/空 trace/连接失败）
- SelfCheck 163 项
- Python SCPI/GRBL 模拟器 + `run_spectrum_simulator_check.ps1`

## 当前风险

| 风险 | 缓解 |
|------|------|
| 真实 SCPI 命令差异 | `config/scpi_profiles/` 可编辑 |
| GRBL 版本差异 | HardwareDebugDialog 原始命令 + session 回放 |
| 误操作真实运动 | safe-mode、Home 确认、Checklist error 阻止 |
| 日志磁盘占满 | cleanupOldLogs(30) + trace 摘要 |

## 本阶段计划（v0.13.0）

1. ✅ HardwareBringupWizard
2. ✅ Session Recorder/Replay
3. ✅ Fault Injection
4. ✅ ScanErrorPolicy 扩展
5. ✅ CLI + 硬件脚本
6. ✅ SelfCheck 扩展至 163
7. ✅ 文档与 CI SelfCheck 步骤

## 完成标准

- [x] build PASS
- [x] SelfCheck 全部 PASS（≥135，实际 163）
- [x] Mock 扫描流程可用
- [x] portable 包可生成
- [x] 硬件文档与 gap report 更新
- [x] working tree clean（提交后）
