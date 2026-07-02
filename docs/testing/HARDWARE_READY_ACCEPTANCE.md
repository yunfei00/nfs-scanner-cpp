# 硬件就绪验收标准

## v0.13.0-hardware-final 软件就绪（不接硬件）

| 验收项 | 标准 | 验证方式 |
|--------|------|----------|
| 构建 | Release PASS | `build_windows_msvc.ps1` |
| 自检 | 163/163 PASS | `NFSScannerSelfCheck.exe` |
| Mock 扫描 | 完整 task 目录 | GUI 或现有 Mock 流程 |
| Profile | 8+1 可加载 | SelfCheck + 设备页 |
| Bring-up 向导 | mock_all 全流程 | 硬件接入向导 |
| 诊断包 | 可导出目录 | Help 菜单 / CLI |
| 会话记录 | JSONL 可回放 | SelfCheck |
| 故障注入 | Mock 可触发失败 | Debug 面板 + SelfCheck |
| Portable | zip 可生成 | `package_portable_windows.ps1` |

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
