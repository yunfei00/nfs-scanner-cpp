# v0.7.0 Spectrum SCPI Framework

## 功能范围

v0.7.0 新增频谱仪抽象接口和 TCP Socket SCPI 通信框架，支持 Mock Spectrum、Generic SCPI 和 R&S ZNA67 初版适配。当前阶段不接 FSW、N9020A、VISA、相机或授权模块。

当前采集流程仍在 UI 线程同步执行，单次 SCPI 查询超时控制在 10 秒以内。后续版本会迁移到异步 worker thread，减少真实仪表慢响应时的界面停顿。

## 支持设备

- Mock Spectrum：无硬件演示和完整扫描流程回归。
- Generic SCPI：用于标准 TCP SCPI 设备的基础连通性测试。
- R&S ZNA67：通过 TCP SCPI 查询 IDN、配置扫频范围并读取 `SDATA`。

## TCP SCPI 连接参数

- Host：仪表 IP 地址，例如 `192.168.0.10`。
- Port：默认 `5025`。

Mock Spectrum 不需要真实网络连接，但 UI 中仍保留 Host/Port 输入，便于同一套操作流程演示。

## 支持命令

Generic SCPI 使用：

- `*IDN?`
- `FREQ:STAR`
- `FREQ:STOP`
- `BAND`
- `SWE:POIN`
- `INIT:IMM`
- `TRAC:DATA? TRACE1`
- `CALC:DATA? SDATA`

R&S ZNA67 初版使用：

- `*IDN?`
- `SENS1:FREQ:STAR`
- `SENS1:FREQ:STOP`
- `SENS1:SWE:POIN`
- `SENS1:BAND`
- `INIT1:CONT OFF`
- `INIT1:IMM`
- `*OPC?`
- `CALC1:DATA? SDATA`

## ZNA67 数据解析

`CALC1:DATA? SDATA` 通常返回 `re,im,re,im...` 成对数据。当前版本将复数数据转换为幅度：

```text
magnitude = sqrt(re^2 + im^2)
```

如果返回数量等于扫描点数，则按实数幅度写入。返回数量不等于 `sweepPoints` 或 `2 * sweepPoints` 时，扫描会报错并停止，避免写坏 `traces.csv`。

## traces.csv 写入

扫描流程优先使用已连接的外部频谱仪。如果未连接真实仪表，则自动使用内部 Mock Spectrum fallback。保存格式仍兼容 v0.4.0：

```text
fre,...
x_y_z_Trc1_S21_re,...
x_y_z_Trc1_S21_im,0,0,0,...
```

当前 `SpectrumTrace.values` 表示幅度，`im` 行仍写 0。多 trace 和复数原始保存后续扩展。

## 常见问题

- 连接超时：确认仪表 IP、网线、子网和防火墙设置。
- 端口不通：确认仪表 SCPI/LAN 服务开启，默认端口通常为 `5025`。
- SCPI 命令不兼容：Generic SCPI 命令集较宽松，不同厂商可能需要专用适配器。
- 返回点数不一致：检查 sweep point 配置，确保仪表返回点数与 UI 配置一致。
- 扫描时界面短暂停顿：当前版本同步查询真实仪表，后续会改为异步采集线程。

## 后续计划

- FSW 适配。
- N9020A 适配。
- VISA 支持。
- 异步采集线程。
- 多 trace 复数保存。
