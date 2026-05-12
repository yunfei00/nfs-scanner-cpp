# Spectrum SCPI Framework

## 功能范围

v0.7.0 新增频谱仪抽象接口和 TCP Socket SCPI 通信框架，支持 Mock Spectrum、Generic SCPI 和 R&S ZNA67 初版适配。

v0.8.0 增加异步采集 worker，并新增 R&S FSW 和 Keysight N9020A 初版 TCP SCPI 适配。当前阶段不接 VISA、相机、授权或复杂插件系统。

## 支持设备

- Mock Spectrum：无硬件演示和完整扫描流程回归。
- Generic SCPI：用于标准 TCP SCPI 设备的基础连通性测试。
- R&S ZNA67：通过 TCP SCPI 查询 IDN、配置扫频范围并读取 `SDATA`。
- R&S FSW：通过 TCP SCPI 查询 IDN、配置频谱扫频并读取 `TRACE1`。
- Keysight N9020A：通过 TCP SCPI 查询 IDN、配置频谱扫频并读取 `TRACE1`。

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

R&S FSW 初版使用：

- `*IDN?`
- `FREQ:STAR`
- `FREQ:STOP`
- `BAND`
- `SWE:POIN`
- `INIT:CONT OFF`
- `INIT:IMM`
- `*OPC?`
- `TRAC:DATA? TRACE1`

Keysight N9020A 初版使用：

- `*IDN?`
- `:FREQ:STAR`
- `:FREQ:STOP`
- `:BAND`
- `:SWE:POIN`
- `:INIT:CONT OFF`
- `:INIT:IMM`
- `*OPC?`
- `:TRAC:DATA? TRACE1`

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

## 异步采集

v0.8.0 中扫描流程通过 `SpectrumAcquisitionWorker` 在独立 `QThread` 中执行 `singleSweep`。ScanManager 不再在主线程 timer tick 中直接采集频谱，而是：

```text
扫描点 -> dwell -> acquireSpectrumRequested -> worker acquire -> acquisitionFinished -> 保存数据
```

如果真实仪表未连接，扫描会自动 fallback 到 Mock Spectrum，并在日志中提示。

## 错误恢复策略

- Timeout：SCPI 查询超时会记录具体命令，例如 `SCPI query timeout: *OPC?`。
- Retry：采集失败后可按 UI 设置重试。
- StopOnError：默认失败即停止，避免写入错位数据。
- 点数不一致：如果 `traces.csv` 已写入频率行，后续 trace 频率点数不一致会拒绝写入。

## 常见问题

- 连接超时：确认仪表 IP、网线、子网和防火墙设置。
- 端口不通：确认仪表 SCPI/LAN 服务开启，默认端口通常为 `5025`。
- SCPI 命令不兼容：Generic SCPI 命令集较宽松，不同厂商可能需要专用适配器。
- 返回点数不一致：检查 sweep point 配置，确保仪表返回点数与 UI 配置一致。
- 扫描时界面短暂停顿：当前版本已经将扫描采集放入 worker 线程；单次扫描按钮仍可能短暂停顿，后续会统一命令队列。
- 采集失败后停止：检查 timeout、retry 和失败停止策略。

## 后续计划

- VISA 支持。
- 更完整的设备线程和命令队列。
- 多 trace 复数保存。
