# Full Mock Validation

一键运行全自动 Mock 验收（无需真实硬件、无需人工点 UI）。

## 用法

在仓库根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/validation/run_full_mock_validation.ps1
```

可选参数：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/validation/run_full_mock_validation.ps1 -SkipBuild
powershell -ExecutionPolicy Bypass -File scripts/validation/run_full_mock_validation.ps1 -Version v0.14.0-mock-validated
```

## 输出

| 路径 | 说明 |
|------|------|
| `validation_output/FULL_MOCK_VALIDATION_REPORT.md` | 完整验收报告 |
| `validation_output/validation_results.json` | 每项 PASS/FAIL/WARN 明细 |
| `validation_output/mock_bringup/` | Bring-up 报告 |
| `validation_output/mock_scan/` | Mock E2E 扫描任务 |
| `validation_output/diagnostics/` | 诊断包副本 |
| `validation_output/sessions/` | Session JSONL |

## 自动验证项

- Build / SelfCheck / Portable 打包
- 9 个 hardware profile JSON 校验
- Mock 运动 / 频谱 / 相机 / 探头
- PreScanChecklist（MockAll）
- Mock bring-up 全流程
- Mock 扫描 E2E（9 点 snake 网格）
- traces.csv 解析 / 热力图 / Alignment / 报告 / 诊断包
- 故障注入（预期错误视为 PASS）
- Session 记录与 replay

## 需真实硬件人工验证

见报告末尾「真实硬件待人工验证」章节。

## 出错排查

1. 查看 `validation_output/FULL_MOCK_VALIDATION_REPORT.md` 的 FAIL 列表
2. 查看 `validation_output/validation_results.json`
3. 扫描失败时检查 `validation_output/mock_scan/` 下最新任务目录
4. 将整个 `validation_output/` 目录打包发给开发人员
