# Keysight N9020A 现场 Bring-up 指南

适用于 NFSScanner **v0.12.0-hardware-ready**，配合 `spectrum.type=n9020a` 或 Profile `spectrum_only_n9020a` / `grbl_n9020a_default`。

## 1. 硬件与网络

- [ ] N9020A / MXA / X-Series 上电  
- [ ] LAN 配置完成，SCPI 端口 **5025**  
- [ ] 连接时驱动会校验 `*IDN?` 必须含 `N9020A`/`MXA`/`X-SERIES` 且厂商为 Keysight/Agilent  

模拟器：

```powershell
python tools/simulators/scpi_spectrum_simulator.py --device n9020a
```

> 若用 `zna67` 或 `fsw` 模拟器连接，N9020A 驱动会在 IDN 校验阶段拒绝连接。

## 2. 软件配置

```json
"spectrum": {
  "enabled": true,
  "type": "n9020a",
  "address": "192.168.0.12",
  "port": 5025,
  "timeout_ms": 8000,
  "retry_count": 2,
  "start_freq_hz": 1000000000,
  "stop_freq_hz": 3000000000,
  "points": 401,
  "rbw_hz": 3000,
  "vbw_hz": 3000,
  "trace": "Trc1_S21"
}
```

Profile：

```powershell
copy config\profiles\spectrum_only_n9020a.json config\hardware_config.json
```

## 3. 驱动行为摘要

`N9020aSpectrumAnalyzer` 特点：

| 项目 | 说明 |
|------|------|
| 连接 | TCP 后立即 `*IDN?` 并校验型号 |
| 配置 | `*CLS`、`FORM ASC`、`FREQ:STAR/STOP`、`BAND:RES/VID`、`SWE:POIN`、`TRACe1:TYPE`、`INIT:CONT OFF` |
| 单次采集 | `ABOR` → `INIT:IMM` → `*OPC?` → `TRAC:DATA? TRACE1` |
| 频率轴 | 查询 `FREQ:STAR?`、`FREQ:STOP?`、`SWE:POIN?` 构建 |

Trace 数据为 ASCII 浮点列表；若长度为 `2×points` 则按复数 re/im 解析，否则按幅度处理。

## 4. 联调步骤

### 4.1 连接与 IDN

- [ ] 设备页选 **N9020A**  
- [ ] 连接成功且日志打印完整 IDN  
- [ ] 若提示「IDN does not look like N9020A」— 检查 `type` 与真机型号  

### 4.2 配置与单次扫描

- [ ] 应用扫描频率/RBW/点数  
- [ ] 单次扫描返回 trace  
- [ ] 点数与 `SWE:POIN` 一致（或驱动日志提示 using actual count）  

### 4.3 扫描任务

- [ ] `grbl_n9020a_default` 联合运动  
- [ ] Checklist 通过  
- [ ] `traces.csv` 验证  

## 5. 仪表侧建议设置

- 参考电平、衰减、前置放大按现场 RF 链路设置（软件 configure 不含 RF 衰减 SCPI）  
- 建议预置 **Trace 1** 为 Write 或 Average，与 `TRACe1:TYPE` 一致  
- 关闭连续扫描 `INIT:CONT OFF`（驱动已发送）  

## 6. 常见问题

| 现象 | 处理 |
|------|------|
| IDN 校验失败 | 确认 `spectrum.type=n9020a`；模拟器用 `--device n9020a` |
| TRACE 空 | 先 `INIT:IMM` 完成后再读；增大 timeout |
| 点数 mismatch 警告 | 通常可继续；检查仪表实际 sweep points |
| FORM 相关乱码 | 驱动已发 `FORM ASC`；避免手动切 BIN |

## 7. 手动 SCPI 验证

```
*IDN?
*CLS
FREQ:STAR 1GHz
FREQ:STOP 3GHz
SWE:POIN 201
INIT:CONT OFF
INIT:IMM
*OPC?
TRAC:DATA? TRACE1
```

## 8. 回退 Mock

```json
"spectrum": { "type": "mock", "enabled": false }
```

## 9. 记录模板

| 项目 | 记录 |
|------|------|
| 序列号 / 固件 | 来自 *IDN? |
| IP | |
| 频率范围 / RBW / Points | |
| TRACe1 类型 | WRIT/AVER/MAXH |
| 首点 trace 长度 | |
| 联调结论 | PASS / FAIL + 现象 |

## 相关文档

- [SCPI_SPECTRUM_CHECKLIST.md](SCPI_SPECTRUM_CHECKLIST.md)  
- [DIAGNOSTIC_PACKAGE_GUIDE.md](DIAGNOSTIC_PACKAGE_GUIDE.md)  
