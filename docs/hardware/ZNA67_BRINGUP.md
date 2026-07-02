# R&S ZNA67 现场 Bring-up 指南

适用于 NFSScanner **v0.12.0-hardware-ready**，配合 `spectrum.type=zna67` 或 Profile `spectrum_only_zna67` / `grbl_zna67_default`。

## 1. 硬件与网络

- [ ] ZNA67 上电，LAN 口连接与 PC 同网段交换机  
- [ ] 仪表 SCPI over LAN 已启用（默认端口 **5025**）  
- [ ] PC 防火墙放行出站 TCP 5025  
- [ ] `ping <仪表IP>` 通  

无真机时可使用模拟器：

```powershell
python tools/simulators/scpi_spectrum_simulator.py --device zna67
# hardware_config: "address": "127.0.0.1"
```

## 2. 软件配置

```json
"spectrum": {
  "enabled": true,
  "type": "zna67",
  "address": "192.168.0.10",
  "port": 5025,
  "timeout_ms": 8000,
  "retry_count": 2,
  "start_freq_hz": 1000000000,
  "stop_freq_hz": 3000000000,
  "points": 201,
  "rbw_hz": 1000,
  "trace": "Trc1_S21"
}
```

或加载 Profile：

```powershell
copy config\profiles\spectrum_only_zna67.json config\hardware_config.json
```

修改 `address` 为实际 IP。

## 3. 驱动行为摘要

`Zna67SpectrumAnalyzer` 使用的 SCPI 序列：

| 阶段 | 命令 |
|------|------|
| 连接 | TCP 5025 |
| 配置 | `SENS1:FREQ:STAR/STOP`、`SENS1:SWE:POIN`、`SENS1:BAND`、`INIT1:CONT OFF` |
| 单次采集（主路径） | `MMEM:STOR:TRAC:CHAN 1, "C:\temp\data.csv"` → `*OPC?` → `MMEM:DATA?` → `MMEM:DEL` |
| 回退路径 | `INIT1:IMM` → `*OPC?` → `CALC1:DATA? SDATA` |
| IDN | `*IDN?` |

CSV 格式要求：首列 `freq`，后续 `re:TraceName` / `im:TraceName` 列（分号分隔）。

## 4. 联调步骤

### 4.1 设备页单项测试

- [ ] 选择类型 **ZNA67**，填入 IP/端口  
- [ ] **连接频谱仪** — 日志显示 connected  
- [ ] **查询 IDN** — 记录完整字符串（应含 `ZNA`）  
- [ ] **应用配置** — 起止频率、RBW、点数  
- [ ] **单次扫描** — 返回 trace，点数 ≈ `points`  

### 4.2 与扫描集成

- [ ] 加载 `grbl_zna67_default` 或保持 motion Mock  
- [ ] 扫描前 Checklist 频谱项 **已连接**  
- [ ] 单点扫描：检查 `traces.csv` 首行频率轴单调递增  
- [ ] 复数 trace：确认 `re`/`im` 或 dB 幅度合理  

### 4.3 故障注入（可选）

```powershell
python tools/simulators/scpi_spectrum_simulator.py --device zna67 --random-fail-rate 0.05 --delay-ms 100
```

验证 `retry_count` 与 UI 错误提示。

## 5. 常见问题

| 现象 | 原因 | 处理 |
|------|------|------|
| 连接超时 | IP/端口/防火墙 | ping、telnet 5025、`timeout_ms` 加大 |
| MMEM 失败 | 仪表无写权限或路径不存在 | 真机创建 `C:\temp`；或等待 SDATA 回退 |
| CSV 解析失败 | trace 名不匹配 | 确认 `trace` 与仪表 Trace 名一致（如 `Trc1_S21`） |
| 点数不一致 | 配置未生效 | 重新 configure 后再 sweep |
| OPC 超时 | 扫描时间过长 | 减小 span/points 或增大 timeout |

## 6. 回退 Mock

```json
"spectrum": { "enabled": false, "type": "mock" }
```

或设备页选择 Mock Spectrum。

## 7. 需记录字段

| 项目 | 示例 |
|------|------|
| IP / 端口 | 192.168.0.10:5025 |
| *IDN? | Rohde & Schwarz,ZNA67,... |
| 配置频率 | 1~3 GHz, 201 points |
| 采集路径 | MMEM CSV / SDATA fallback |
| 首点 trace 行数 | 201 |
| 失败 SCPI 摘要 | UI 日志最后 5 行 |

## 相关文档

- [SCPI_SPECTRUM_CHECKLIST.md](SCPI_SPECTRUM_CHECKLIST.md)  
- [HARDWARE_CONFIG_PROFILES.md](HARDWARE_CONFIG_PROFILES.md)  
- [docs/spectrum_scpi.md](../spectrum_scpi.md)  
