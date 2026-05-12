# v0.9.0 Python 真实仪表逻辑迁移

v0.9.0 以已经过真实测试的 Python 工程为依据，修正 C++ 版真实扫描链路。重点是三类仪表的采集方式不再只依赖泛化 SCPI 猜测，而是迁移现场可用流程。

## ZNA67 MMEM 流程

ZNA67 优先使用仪表端 CSV 导出：

```text
MMEM:STOR:TRAC:CHAN 1, "C:\temp\data.csv"
*OPC?
MMEM:DATA? "C:\temp\data.csv"
MMEM:DEL "C:\temp\data.csv"
*OPC?
```

返回 CSV 为分号格式：

```text
freq[Hz];re:Trc1_S21;im:Trc1_S21;re:Trc2_S31;im:Trc2_S31
1000000;0.1;0.2;0.3;0.4
```

C++ 解析后保留所有 trace component。主显示值使用第一个完整 re/im trace 的 dB magnitude：

```text
20 * log10(max(sqrt(re^2 + im^2), 1e-12))
```

如果 MMEM 失败，会尝试一次 `CALC1:DATA? SDATA` fallback。

## FSW MMEM 流程

FSW 使用 MMEM CSV 导出，临时路径为 `C:\data.csv`：

```text
DISP:TRAC1:MODE WRIT
DISP:TRAC1:MODE MAXH
DISP TRAC1 ON
:FORM:DEXP:DSEP POIN
:FORM:DEXP:FORM CSV
MMEM:STOR1:TRAC 1,"C:\data.csv"
*OPC?
MMEM:DATA? "C:\data.csv"
```

CSV 每行按 `frequency, amplitude` 解析。频率轴优先使用 CSV 第一列，幅度直接写入 `Trc1_S21_re`，`im` 行写 0。

## N9020A ASCII TRACE 流程

N9020A 连接后立即查询 `*IDN?` 并校验：

- model 包含 `N9020A`、`MXA` 或 `X-SERIES`
- vendor 包含 `KEYSIGHT` 或 `AGILENT`

配置前发送：

```text
*CLS
FORM ASC
```

采集流程：

```text
FORM ASC
ABOR
INIT:IMM
*OPC?
TRAC:DATA? TRACE1
```

返回值优先按逗号 ASCII 幅值解析；如果数量为 `2 * sweepPoints`，按 re/im 成对转 magnitude，并保留 real/imag。

## traces.csv 多 Trace 格式

任务存储保持旧 Python 兼容格式：

```text
fre,f1,f2,...
x_y_z_Trc1_S21_re,...
x_y_z_Trc1_S21_im,...
x_y_z_Trc2_S31_re,...
x_y_z_Trc2_S31_im,...
```

第一条 trace 决定频率行；后续 trace 的频率点数不一致时会停止写入并报错，避免损坏文件。

## 真实运动扫描流程

扫描状态机支持真实运动控制：

1. 发送 `G1X..Y..Z..F..` 到运动控制器。
2. 每 150 ms 查询位置。
3. 等待状态包含 `Idle` 且 `MPos` 与目标点误差小于 0.05 mm。
4. 等待 dwell 时间。
5. 异步采集频谱。
6. 保存 `points.csv` 和 `traces.csv`。

如果未勾选模拟模式但运动串口未连接，主界面会阻止开始扫描并提示先打开串口或启用模拟模式。

## 现场测试建议

1. 先用 Mock Spectrum 和模拟运动跑通小区域扫描。
2. 分别连接 ZNA67、FSW、N9020A，执行查询 IDN、应用配置、单次扫描。
3. 再执行 2x2 小区域扫描。
4. 检查 `traces.csv` 是否包含对应 trace 行。
5. 加载数据并显示热力图。
