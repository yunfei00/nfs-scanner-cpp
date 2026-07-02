# 真实硬件现场联调总指南

## 适用范围

本文档适用于 **v0.11.0-hardware-alpha** 及之后版本。软件层已预实现 Mock / 真实双路径，**未接硬件时** 仍可启动、Mock 扫描、SelfCheck。

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

## Mock / Real 切换

| 子系统 | Mock 条件 | Real 条件 |
|--------|-----------|-----------|
| 运动 | 设备页勾选「模拟模式」 | 取消模拟 + `motion.enabled=true` + 串口连接 |
| 频谱 | `spectrum.type=mock` 或未选真实仪表 | `spectrum.enabled=true` + TCP 连接成功 |
| 相机 | `camera.type=mock` | USB/工业 SDK 待接入；Mock 可拍照 |
| 探头 | `probe.type=mock` | Serial/SDK Stub 待现场确认 |

## 推荐联调顺序

1. 加载/保存 `hardware_config.json`（设备页 → 硬件配置）
2. 单项测试：运动 → 频谱 → 相机 → 探头（设备页「设备单项测试」）
3. 运行 **Help → 诊断信息**，导出 `logs/diagnostics_YYYYMMDD_HHMMSS.md`
4. 扫描前 Checklist：点击「开始扫描」后查看检查结果
5. 单点扫描验证 traces.csv
6. 小区域网格扫描

## 日志位置

- UI 日志区：主窗口底部
- 诊断导出：`logs/diagnostics_*.md`（相对可执行文件 `../logs`）
- 扫描任务：`project/scans/<task>/points.csv`、`traces.csv`、`scan_config.json`

## 失败回退 Mock

1. 设备页勾选「模拟模式」（运动）
2. `hardware_config.json` 中设置 `spectrum.type: mock`
3. 断开全部设备（设备页单项测试区）
4. 重新运行 SelfCheck 确认软件正常

## 需人工记录的结果

| 项目 | 记录内容 |
|------|----------|
| 串口 | COM 口、波特率、$I/?/$H 响应 |
| 频谱仪 | *IDN?、单次 sweep、trace 行数 |
| 相机 | 拍照路径、Alignment 背景是否正常 |
| 探头 | Hx/Hy 切换延时、scan_config 中 probe_orientation |
| 扫描 | 首点坐标、首行 trace、失败重试次数 |

## 子文档

- [GRBL_MOTION_CHECKLIST.md](GRBL_MOTION_CHECKLIST.md)
- [SCPI_SPECTRUM_CHECKLIST.md](SCPI_SPECTRUM_CHECKLIST.md)
- [CAMERA_CHECKLIST.md](CAMERA_CHECKLIST.md)
- [PROBE_HX_HY_CHECKLIST.md](PROBE_HX_HY_CHECKLIST.md)

## 当前限制（未接硬件）

- USB / 工业相机为 Stub，不引入 OpenCV
- 探头 Serial/SDK 为 Stub，Hx/Hy 仅 Mock 可切换
- 真实 GRBL / SCPI 需现场 IP/COM 配置，软件不写死
- 启动时 **不会** 自动连接任何真实设备
