# 硬件就绪验收标准

## v0.14.0-mock-validated 软件就绪（不接硬件）

| 验收项 | 标准 | 验证方式 |
|--------|------|----------|
| 构建 | Release PASS | `build_windows_msvc.ps1` |
| 自检 | 163/163 PASS | `NFSScannerSelfCheck.exe` |
| **全自动 Mock 验收** | 报告结论 PASS / PASS_WITH_HARDWARE_WARNINGS | `scripts/validation/run_full_mock_validation.ps1` |
| Mock 扫描 E2E | 9 点 snake + traces.csv | `NFSScannerCli --run-mock-e2e` |
| Profile | 9 个可校验 | `NFSScannerCli --validate-profiles` |
| Bring-up 向导 | mock_all 全流程 | CLI / 硬件接入向导 |
| 诊断包 | 可导出 | CLI / Help 菜单 |
| 会话记录 | JSONL 可回放 | CLI validation |
| 故障注入 | 预期错误识别为 PASS | CLI validation |
| Portable | zip 可生成 | validation 脚本末尾 |

## 现场硬件就绪（需人工）

| 验收项 | 标准 |
|--------|------|
| GRBL | Home/点动/限位 OK |
| SCPI | *IDN? + single sweep + trace 解析 |
| 相机 | 真实 capture（SDK 接入后） |
| 探头 | Hx/Hy 切换可靠 |
| 完整扫描 | 真实模式 checklist 无 error |

## 推荐签字顺序

1. 软件就绪（本表上半部分）— 开发/QA  
2. 分项硬件（motion / spectrum / camera / probe）— 现场工程师  
3. 完整扫描验收 — 项目负责人  
