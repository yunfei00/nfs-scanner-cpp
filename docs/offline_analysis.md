# 离线分析说明

## v0.5.0 功能范围

v0.5.0 支持加载 v0.4.0 生成的 `traces.csv`，自动解析频率列表、Trace 列表和扫描坐标点，并根据指定 Trace、频率点和显示模式生成基础热力图。

本阶段不接真实频谱仪、不接相机、不做图片叠加、不做复杂 LUT/Colorbar、不使用 SQLite。

## traces.csv 格式

第一行是频率轴：

```text
fre,1000000,5995000,...
```

后续每个扫描点通常有两行：

```text
x_y_z_Trc1_S21_re,value1,value2,value3,...
x_y_z_Trc1_S21_im,0,0,0,...
```

如果只有 `_re` 或只有 `_im`，缺失部分会按 0 处理。

## Key 解析规则

行首 key 使用 `_` 分割：

- `parts[0]`：x
- `parts[1]`：y
- `parts[2]`：z
- `parts[-1]`：`re` 或 `im`
- `parts[3:-1]`：重新用 `_` 拼接为 traceId

示例：

```text
0_0_1_Trc1_S21_re -> traceId = Trc1_S21
0_0_1_trace1_im -> traceId = trace1
```

## Trace 自动发现

解析器遍历所有有效数据行，将 key 中的 traceId 去重并排序后填充到 Trace 下拉框。

## 频率点选择

频率列表来自 `fre` 行。UI 中根据频率大小自动显示为 GHz、MHz、kHz 或 Hz，并保留真实频率索引用于热力图生成。

## 显示模式

- 幅度：`abs(complex)`
- 幅度dB：`20 * log10(abs(complex) + 1e-12)`
- 相位：`atan2(imag, real)`
- 实部：`real`
- 虚部：`imag`

## 基础热力图流程

1. `FrequencyCsvParser` 解析 `traces.csv`。
2. `FrequencyData` 保存频率、Trace、坐标和复数频谱数据。
3. `HeatmapGenerator` 按 Trace、频率索引和显示模式生成二维矩阵。
4. 自动计算 `vmin/vmax` 并映射为基础 jet-like 色带。
5. `HeatmapDialog` 弹窗显示 QImage，并支持导出 PNG。

## v0.6.0 计划

- LUT 选择
- Colorbar
- vmin/vmax 手动设置
- 主视图叠加显示
