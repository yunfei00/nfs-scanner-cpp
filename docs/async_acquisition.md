# v0.8.0 Async Spectrum Acquisition

## 目标

v0.8.0 将扫描过程中的频谱采集从主线程同步调用改为 worker 线程执行，降低真实仪表响应慢、`*OPC?` 等待或 TCP 查询超时时对 UI 的影响。同时新增采集超时、重试次数和失败停止策略，并保留 Mock Spectrum fallback。

## 为什么要异步采集

真实频谱仪一次 sweep 可能需要数百毫秒到数秒。v0.7.0 中 `ScanManager` 在 timer tick 中直接调用 `singleSweep`，真实设备慢响应时会阻塞主线程。v0.8.0 将 `singleSweep` 放入 `SpectrumAcquisitionWorker`，由 `QThread` 执行，让界面、日志和按钮状态保持响应。

## ScanManager 与 Worker

`ScanManager` 负责：

- 生成扫描路径。
- 推进扫描状态。
- 发起采集请求。
- 接收采集结果。
- 保存 `points.csv` 和 `traces.csv`。

`SpectrumAcquisitionWorker` 负责：

- 选择外部真实频谱仪或 Mock fallback。
- 执行 `ISpectrumAnalyzer::singleSweep`。
- 检查返回的频率点和数据点。
- 按配置进行失败重试。
- 通过信号返回 `SpectrumAcquisitionResult`。

## 采集状态流程

```text
processNextPoint
  -> dwell timer
  -> acquireSpectrumRequested
  -> SpectrumAcquisitionWorker::acquire
  -> acquisitionFinished
  -> appendPoint / appendTrace
  -> processNextPoint
```

每个点采集成功后才进入下一个点，避免数据和坐标错位。

## Pause / Stop 行为

- 暂停：如果正在采集，当前采集允许完成；完成后停在下一个点之前。
- 继续：恢复 `Running` 状态并继续处理下一个点。
- 停止：不再发起新采集；如果晚到的采集结果返回，ScanManager 会忽略它，并保留已写入的部分数据。
- 析构：ScanManager 会安全退出采集线程，避免 `QThread destroyed while thread is still running`。

## Timeout / Retry / StopOnError

当前 UI 提供：

- 超时(ms)：默认 10000，范围 1000 到 60000。
- 重试次数：默认 1，范围 0 到 5。
- 失败停止：默认开启。

采集失败包括：

- 频谱仪不可用。
- 查询超时。
- 返回空数据。
- `freqs.size()` 与 `values.size()` 不一致。
- 写入 `traces.csv` 时频率点数与首行不一致。

`stopOnError=true` 时，扫描立即进入错误状态并停止。`stopOnError=false` 时，可以跳过失败点并继续后续点。

## 后续优化方向

- 将真实 SCPI 设备对象也迁移到设备专用线程。
- 增加命令队列，统一串行化 `configure/query/sweep`。
- 将 `ScpiTcpClient` 改为真正异步 socket 状态机。
- 增加扫描恢复、失败点重扫和采集质量统计。
