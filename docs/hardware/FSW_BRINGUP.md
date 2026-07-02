# R&S FSW 现场 Bring-up 指南

适用于 NFSScanner **v0.12.0-hardware-ready**，配合 `spectrum.type=fsw` 或 Profile `spectrum_only_fsw` / `grbl_fsw_default`。

## 1. 硬件与网络

- [ ] FSW 系列频谱仪上电，LAN 连接正常  
- [ ] Remote / SCPI over LAN 开启，端口 **5025**  
- [ ] PC 与仪表同网段  

模拟器联调：

```powershell
python tools/simulators/scpi_spectrum_simulator.py --device fsw
```

## 2. 软件配置

```json
"spectrum": {
  "enabled": true,
  "type": "fsw",
  "address": "192.168.0.11",
  "port": 5025,
  "timeout_ms": 8000,
  "retry_count": 2,
  "start_freq_hz": 1000000000,
  "stop_freq_hz": 6000000000,
  "points": 801,
  "rbw_hz": 10000,
  "vbw_hz": 10000,
  "trace": "Trc1_S21"
}
```

Profile：

```powershell
copy config\profiles\grbl_fsw_default.json config\hardware_config.json
```

## 3. 驱动行为摘要

`FswSpectrumAnalyzer` 流程：

| 阶段 | 命令 |
|------|------|
| 配置 | `FREQuency:STARt/STOP`、`BANDwidth:RESolution/VIDeo`、`SWEep:POINts`、`INIT:CONT OFF` |
| 单次采集 | `DISP:TRAC1:MODE WRIT` → settle → `MAXH`/`AVER` 等 → `MMEM:STOR1:TRAC 1,"C:\data.csv"` → `*OPC?` → `MMEM:DATA?` |
| IDN | `*IDN?` |

CSV 为频率 + 幅度两列（`,` 或 `;` 分隔均可解析）。

默认临时文件路径：`C:\data.csv`（代码内 `mmemTempTracePath_`）。

## 4. 联调步骤

### 4.1 单项测试

- [ ] 设备页类型选 **FSW**  
- [ ] 连接 + **IDN**（应含 `FSW`）  
- [ ] 应用配置（注意 FSW 常需更大 `timeout_ms`）  
- [ ] 单次扫描 — 频率/幅度数组非空  

### 4.2 Trace 模式

FSW 支持 `WRIT` / `MAXH` / `AVER` / `MINH`。扫描配置中的 trace 模式会映射到 `DISP:TRAC1:MODE`。

- [ ] 确认当前模式符合测试需求（一般扫描用 MAXH 或 WRIT）  
- [ ] `clearWriteSettleSeconds` 内建延时后切换模式  

### 4.3 联合扫描

- [ ] Profile `grbl_fsw_default` + GRBL 真机/模拟器  
- [ ] 小区域 2×2 网格扫描  
- [ ] `traces.csv` 每点一行，频率列一致  

## 5. 常见问题

| 现象 | 处理 |
|------|------|
| MMEM store 失败 | 确认仪表允许写 `C:\data.csv`；检查磁盘空间 |
| CSV 无有效行 | 先手动 SCPI 存 trace 对比格式 |
| 幅度全 0 | 检查 RF 输入/参考电平（真机）；模拟器应返回伪随机 dB |
| 连接被 RST | 避免与其他 SCPI 客户端争用同一连接 |

## 6. 真机 SCPI 快测（可选）

使用 Python 或 NI MAX/VISA 终端：

```
*IDN?
FREQuency:STARt 1e9
FREQuency:STOP 3e9
SWEep:POINts 201
INIT:IMM;*OPC?
```

## 7. 回退 Mock

`spectrum.type: mock` 或 `spectrum_only` profile 中 motion 保持 Mock。

## 8. 记录模板

| 字段 | 值 |
|------|-----|
| 型号 / 选件 | FSWxx + Kxx |
| IP:Port | |
| IDN | |
| RBW / VBW / Points | |
| Trace 模式 | MAXH / WRIT |
| 首点 trace 点数 | |
| 备注 | |

## 相关文档

- [SCPI_SPECTRUM_CHECKLIST.md](SCPI_SPECTRUM_CHECKLIST.md)  
- [REAL_HARDWARE_BRINGUP_GUIDE.md](REAL_HARDWARE_BRINGUP_GUIDE.md)  
