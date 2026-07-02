# NFSScanner 硬件模拟器

v0.12.0 提供的 Python 模拟器，用于在无真实 GRBL 控制器和 SCPI 频谱仪时联调 NFSScanner 真实设备路径（非 Mock UI 模式）。

## 环境要求

- Python 3.8+
- 仅标准库，无需 `pip install`

## 快速启动

在项目根目录执行：

```powershell
# 终端 1 — GRBL 运动（TCP 7001）
python tools/simulators/grbl_motion_simulator.py

# 终端 2 — SCPI 频谱仪（TCP 5025，ZNA67 模式）
python tools/simulators/scpi_spectrum_simulator.py --device zna67

# FSW 模式
python tools/simulators/scpi_spectrum_simulator.py --device fsw

# N9020A 模式
python tools/simulators/scpi_spectrum_simulator.py --device n9020a
```

## GRBL 模拟器

| 参数 | 默认 | 说明 |
|------|------|------|
| `--host` | `0.0.0.0` | 监听地址 |
| `--port` | `7001` | TCP 端口 |
| `--move-delay-ms` | `50` | 每次移动最小延时（毫秒） |

### 支持的命令

与 `SerialMotionController` / `GrblCommandBuilder` 一致：

| 命令 | 行为 |
|------|------|
| `$I` | 返回模拟固件版本 |
| `$H` | 回零至 (0, 0, 0) |
| `$X` | 解除 Alarm |
| `?` | 状态报告，含 `MPos:x,y,z` |
| `G1X..Y..Z..F..` | 绝对移动 |
| `G91G1X..F..` | 相对点动 |
| `!` | Feed Hold |
| `~` | 恢复 |
| `0x18` | Soft reset |

### 坐标限位

与软件 `SerialMotionController::validatePosition` 相同：

- X: 0 ~ 200 mm
- Y: -300 ~ 0 mm
- Z: 0 ~ 10 mm

越界移动返回 `error:15`。

---

## SCPI 模拟器

| 参数 | 默认 | 说明 |
|------|------|------|
| `--host` | `0.0.0.0` | 监听地址 |
| `--port` | `5025` | SCPI TCP 端口 |
| `--device` | `zna67` | `zna67` / `fsw` / `n9020a` |
| `--delay-ms` | `5` | 每条命令处理延时 |
| `--random-fail-rate` | `0` | 查询随机返回空（0~1） |
| `--disconnect-rate` | `0` | 查询后随机断连（0~1） |

### 各模式差异

| 模式 | *IDN? | 主要采集路径 |
|------|-------|--------------|
| `zna67` | R&S ZNA67 | `MMEM:STOR/DATA` CSV（re/im）+ `CALC1:DATA? SDATA` 回退 |
| `fsw` | R&S FSW | `MMEM:STOR1:TRAC` + CSV |
| `n9020a` | Keysight N9020A | `TRAC:DATA? TRACE1` |

### 故障注入示例

```powershell
# 10% 查询失败 + 2% 断连，用于验证重试与错误提示
python tools/simulators/scpi_spectrum_simulator.py --device zna67 --random-fail-rate 0.1 --disconnect-rate 0.02 --delay-ms 50
```

---

## Windows 串口桥接（com0com）

NFSScanner 运动控制器走 **串口**（`QSerialPort`），GRBL 模拟器走 **TCP**。需将二者桥接：

### 方案 A：com0com + com2tcp（推荐）

1. 安装 [com0com](https://sourceforge.net/projects/com0com/)，创建虚拟串口对，例如 **COM10 ↔ COM11**。
2. 安装 [com2tcp](https://sourceforge.net/projects/com0tcp/)（或 Hub4Com 套件中的 com2tcp）。
3. 启动 GRBL 模拟器（7001）。
4. 桥接 TCP → COM10：

   ```powershell
   com2tcp --baud 115200 \\.\COM10 127.0.0.1 7001
   ```

5. NFSScanner 配置 `motion.port = "COM11"`，`baudrate = 115200`，取消「模拟模式」。

### 方案 B：socat（WSL / Linux 联调）

```bash
socat TCP-LISTEN:7001,reuseaddr FILE:/dev/ttyVNCA0,b115200,raw,echo=0
python tools/simulators/grbl_motion_simulator.py --port 7002
# 再用 socat 将 7001 转发到 7002，或直接改模拟器端口
```

### 频谱仪

频谱仪驱动使用 **TCP 5025**，无需 com0com。在 `hardware_config.json` 中设置：

```json
"spectrum": {
  "enabled": true,
  "type": "zna67",
  "address": "127.0.0.1",
  "port": 5025
}
```

---

## 推荐联调 Profile

| 步骤 | Profile | 模拟器 |
|------|---------|--------|
| 1 | `mock_all` | 无需 |
| 2 | `motion_only_grbl` | GRBL + com2tcp |
| 3 | `spectrum_only_zna67` | SCPI `--device zna67` |
| 4 | `grbl_zna67_default` | 两者同时 |
| 5+ | 见 `docs/hardware/HARDWARE_CONFIG_PROFILES.md` | 按需 |

---

## 验证清单

- [ ] GRBL：`$I`、`?`（MPos）、小步 G1、越界 error
- [ ] SCPI：`*IDN?`、configure、单次 sweep、trace 点数与配置一致
- [ ] NFSScanner 设备页单项测试全部通过
- [ ] 导出诊断包（见 `docs/hardware/DIAGNOSTIC_PACKAGE_GUIDE.md`）

## 常见问题

| 现象 | 处理 |
|------|------|
| COM 口找不到 | 确认 com0com 对已创建；设备管理器中查看 COM 号 |
| 连接 7001 失败 | 防火墙放行；确认模拟器已启动 |
| N9020A IDN 校验失败 | 必须使用 `--device n9020a` |
| ZNA MMEM 解析失败 | 确认模拟器为 zna67 模式；检查 `C:\temp\data.csv` 路径（软件默认） |
