# UI 手工测试清单

> v0.13.0-hardware-final · 不接真实硬件可完成 Mock 项

## 启动与导航

- [ ] 启动 NFSScanner.exe，主窗口 1600×900
- [ ] 左侧四页：扫描 / 设备 / 分析 / 报告
- [ ] Help → 诊断信息 / 导出诊断包 / 硬件调试面板

## Profile 与设备

- [ ] 设备页加载 `mock_all` Profile
- [ ] Profile 校验无 error
- [ ] 设备 → **硬件接入向导** Mock 全流程 PASS
- [ ] DevicePage 设备状态与单项测试按钮可用

## 调试工具

- [ ] HardwareDebugDialog 四 Tab + Fault Injection
- [ ] 故障注入 `connect_fail` 后 Mock 运动连接失败
- [ ] 打开 SCPI 日志 / 导出 bring-up 报告

## 扫描与分析

- [ ] 扫描页显示 hardware_mode
- [ ] Mock 扫描完成，生成 points.csv / traces.csv
- [ ] 分析页加载 traces.csv，热力图显示
- [ ] 报告导出 HTML/Markdown

## Alignment

- [ ] AlignmentEditor 线性矩形模式保存 alignment.json
- [ ] 四点透视模式保存 alignment.json

## CLI 与脚本

- [ ] `NFSScanner.exe --profile mock_all --safe-mode` 启动
- [ ] `scripts/hardware/run_mock_all_check.ps1` PASS

## 待真实硬件

- [ ] GRBL 串口运动
- [ ] SCPI 真实仪表
- [ ] USB/工业相机
- [ ] 探头 Hx/Hy 现场切换
- [ ] Installer 安装验证
