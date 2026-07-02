# Mock 端到端测试

## 前置

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_windows_msvc.ps1
$env:PATH = "C:/Qt/6.8.3/msvc2022_64/bin;" + $env:PATH
```

## 自动化（推荐）

```powershell
powershell -ExecutionPolicy Bypass -File scripts/validation/run_full_mock_validation.ps1
```

报告：`validation_output/FULL_MOCK_VALIDATION_REPORT.md`

或分步：

```powershell
.\build\Release\NFSScannerSelfCheck.exe
.\build\Release\NFSScannerCli.exe --profile mock_all --profile-dir config/profiles --run-full-validation --output validation_output
scripts\hardware\run_mock_all_check.ps1
```

预期：SelfCheck 163/163 PASS；CLI 验证 17+ 项 PASS/WARN

## GUI Mock 流程

1. 启动 `NFSScanner.exe --profile mock_all`
2. 设备 → 硬件接入向导 → 运行全部 → 导出报告
3. 扫描页开始 Mock 扫描（2×2 或默认路径）
4. 分析页加载 task 目录 traces.csv
5. Help → 导出诊断包

## 故障注入

1. Help → 硬件调试面板 → Fault Injection
2. 启用 `spectrum_empty_trace`
3. 设备页 Run Bring-up Test → 应显示 trace 失败（Mock）

## 会话回放

1. 运行 bring-up 或扫描
2. 检查 `logs/hardware_sessions/session_*.jsonl`
3. SelfCheck 已覆盖 recorder/replay
