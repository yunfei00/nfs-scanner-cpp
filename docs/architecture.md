# NFS Scanner C++ 商业版架构设计

## 架构目标

本工程面向商业版近场扫描系统客户端，核心目标是把 UI、扫描编排、设备接入、数据分析和基础设施能力解耦。首阶段先交付可运行的 C++17/Qt6 Widgets 骨架与 Mock 扫描流程，后续每个硬件和算法模块都可以在不重写 UI 主体的前提下替换实现。

## 分层设计

### UI 层

位置：`src/ui`

职责：

- 使用 Qt Widgets 构建主窗口、控制面板、日志区、状态栏和热力图展示区。
- 将用户操作转换为 Core 层命令，例如开始扫描、停止扫描、修改扫描参数。
- 订阅扫描进度、设备状态、日志消息并刷新界面。
- `HeatmapView` 当前绘制 Mock 相机区域和模拟热力图，后续通过 `setHeatmapImage()` 接收 Analysis 层生成的 `QImage` 叠加图。

后续扩展：

- 增加项目管理、扫描模板、结果浏览、数据导出等页面。
- 增加相机实时预览、热力图透明度/LUT/阈值控制。
- 增加多语言、主题、快捷键和用户偏好配置。

### Core 层

位置：`src/core`

职责：

- 定义扫描配置、扫描点和扫描流程控制。
- `ScanWorker` 继承 `QObject`，使用 `QTimer` 模拟扫描点推进，保证 UI 线程不被阻塞。
- 对外发出 `progressChanged`、`logMessage`、`scanFinished` 等信号。

后续扩展：

- 将扫描流程升级为任务状态机，支持启动、暂停、恢复、停止、异常恢复和断点续扫。
- 引入扫描路径规划，例如蛇形扫描、分区扫描、自定义 ROI。
- 与 Device 层协作完成“移动到点位 -> 采集频谱 -> 采集图像 -> 写入数据”的真实流程。
- 在需要时把耗时任务迁移到 `QThread` 或线程池。

### Device 层

位置：`src/devices`

职责：

- 定义硬件抽象接口，当前包含运动控制器和频谱仪。
- 提供 Mock 实现，用于 UI 和流程联调。
- 隔离厂商 SDK、通信协议、错误码和设备状态。

后续扩展：

- `IMotionController` 扩展回零、限位、急停、速度/加速度、坐标系和安全区域。
- `ISpectrumAnalyzer` 扩展 Trace 读取、RBW/VBW、Detector、Marker、Sweep 控制。
- 新增 `ICameraDevice`，封装相机连接、曝光、触发、帧回调和图像缓存。
- 为真实硬件增加适配器，例如串口/网口控制器、VISA 频谱仪、厂商相机 SDK。

### Analysis 层

位置：`src/analysis`

职责：

- 负责扫描数据到热力图、统计结果和可视化图像的转换。
- 当前 `HeatmapGenerator` 提供 Mock 热力图生成能力，供 UI 骨架展示。

后续扩展：

- 输入真实扫描点数据，生成插值后的二维场强矩阵。
- 支持 LUT、透明度、阈值、平滑、归一化和多 Trace 对比。
- 输出 `QImage`、CSV、JSON、报告图片或后续数据库记录。
- 支持相机底图与热力图坐标标定、配准和叠加。

### Infra 层

位置：`src/infra`

职责：

- 提供日志、配置、错误处理、路径管理、授权、诊断等基础设施能力。
- 当前 `Logger` 提供统一日志格式化入口。

后续扩展：

- 增加文件日志、运行日志轮转、崩溃诊断和审计日志。
- 增加授权模块，支持本地 license、在线激活、离线授权和功能开关。
- 增加配置服务，管理设备 profile、用户偏好、扫描模板和最近项目。
- 增加打包发布辅助，例如版本信息、更新通道、依赖检查。

## 运行流程

1. `main.cpp` 创建 `QApplication`，加载 QSS，启动 `MainWindow`。
2. `MainWindow` 创建 `AppContext`，由 `AppContext` 持有 `ScanWorker` 和 Mock 设备。
3. 用户点击“开始扫描”后，UI 读取扫描参数并设置到 `ScanWorker`。
4. `ScanWorker` 使用 `QTimer` 每 100ms 产生一个 `ScanPoint`。
5. UI 收到进度信号后刷新进度条、日志区、状态栏和 `HeatmapView`。
6. 扫描完成或停止后，`ScanWorker` 发出 `scanFinished`，UI 恢复控制状态。

## 扩展原则

- UI 不直接依赖具体硬件 SDK，只依赖 Device 接口。
- Core 不承担绘图职责，只负责扫描状态和数据流。
- Analysis 不操作设备，只消费扫描数据并生成结果。
- Infra 保持横向能力，避免业务层重复实现日志、配置和授权。
- 所有长耗时操作必须通过异步机制执行，避免阻塞 Qt UI 线程。
