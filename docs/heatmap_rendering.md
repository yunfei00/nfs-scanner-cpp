# v0.6.0 Heatmap Rendering

## 功能范围

v0.6.0 聚焦离线热力图显示增强，不接入真实频谱仪、相机、图片对齐、SQLite 或授权模块。热力图数据仍来自 v0.4.0 任务目录中的 `traces.csv`，由 v0.5.0 的 `FrequencyCsvParser` 加载。

## HeatmapGenerator 输入输出

`HeatmapGenerator` 接收 `FrequencyData` 和 `HeatmapRenderOptions`，输出 `HeatmapRenderResult`。

输出内容包括：

- `image`：渲染后的热力图图像。
- `colorbar`：对应 LUT 和透明度的竖向色带。
- `actualVmin` / `actualVmax`：最终使用的显示范围。
- `ok` / `error`：渲染结果和错误信息。

## HeatmapRenderOptions 字段

- `traceId`：要显示的 Trace，例如 `Trc1_S21`。
- `freqIndex`：频率点索引。
- `mode`：显示模式。
- `lutName`：LUT 名称。
- `autoRange`：是否自动根据有效数据计算范围。
- `vmin` / `vmax`：手动范围。
- `alpha`：热力图透明度，范围 0 到 255。
- `width` / `height`：输出图像尺寸。

## 显示模式

- 幅度：`magnitude`
- 幅度dB：`db`
- 相位：`phase`
- 实部：`real`
- 虚部：`imag`

## 支持的 LUT

- `gray`
- `jet`
- `turbo`
- `viridis`
- `plasma`
- `inferno`
- `magma`
- `hot`
- `cool`
- `rainbow`

## vmin/vmax

自动范围开启时，系统遍历当前 Trace、频率点和显示模式下的有效网格数据，自动计算 `actualVmin` 和 `actualVmax`。

手动范围开启时，使用界面输入的 `vmin` 和 `vmax`。如果 `vmin >= vmax` 或范围过小，渲染器会自动扩展到一个可显示范围，避免除零和全图单色。

## Colorbar

`LutManager::createColorbar` 生成竖向色带，顶部表示高值，底部表示低值。主界面结果区和弹窗预览都会显示当前色带，并标注实际 `vmax` 和 `vmin`。

## 后续计划

v0.7.0 计划接入真实频谱仪框架，包括 ZNA67、FSW、N9020A 的适配准备，以及实时扫描频谱数据采集。
