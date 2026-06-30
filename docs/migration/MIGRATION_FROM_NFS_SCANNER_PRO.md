# NFS Scanner Pro → NFS Scanner C++ 迁移审计

> 生成日期：2026-06-30  
> 源规范仓库：[nfs-scanner-pro](https://github.com/yunfei00/nfs-scanner-pro)  
> 目标工程：[nfs-scanner-cpp](https://github.com/yunfei00/nfs-scanner-cpp) v0.9.0

---

## 1. nfs-scanner-cpp 当前已具备能力清单

### 1.1 核心扫描与运动

| 模块 | 能力 | 关键文件 |
|------|------|----------|
| 扫描配置 | ScanConfig、ScanPoint、蛇形路径规划 | `src/core/ScanConfig.*`, `ScanPathPlanner.*` |
| 扫描状态机 | Idle/Preparing/Running/Paused/Stopping/Stopped/Finished/Error | `src/core/ScanManager.*` |
| 真实运动 | GRBL-like 串口、点动、G1 绝对移动、MPos 解析、Mock fallback | `src/devices/motion/SerialMotionController.*` |
| 扫描流程 | 开始/暂停/继续/停止、进度、剩余点数、预计时间 | `ScanManager` + `MainWindow` |
| 异步采集 | SpectrumAcquisitionWorker 独立线程、超时/重试/stopOnError | `src/devices/spectrum/SpectrumAcquisitionWorker.*` |

### 1.2 频谱仪与数据

| 模块 | 能力 | 关键文件 |
|------|------|----------|
| 统一接口 | ISpectrumAnalyzer、Mock/Generic SCPI | `ISpectrumAnalyzer.h`, `MockSpectrumAnalyzer.*` |
| 真实驱动 | ZNA67 (MMEM CSV re/im)、FSW (MMEM CSV)、N9020A (ASCII TRACE) | `Zna67*`, `Fsw*`, `N9020a*` |
| 工厂 | SpectrumAnalyzerFactory 可选仪表列表 | `SpectrumAnalyzerFactory.*` |
| 任务存储 | meta.json、scan_config.json、points.csv、traces.csv（多 trace re/im） | `src/storage/TaskStorage.*` |

### 1.3 分析与可视化

| 模块 | 能力 | 关键文件 |
|------|------|----------|
| CSV 解析 | traces.csv 加载、Trace 自动发现 | `FrequencyCsvParser.*`, `FrequencyData.*` |
| 热力图 | QImage 整图生成、LUT、Colorbar、vmin/vmax、透明度 | `HeatmapGenerator.*`, `LutManager.*` |
| 预览 | HeatmapView（QWidget paintEvent 整图）、HeatmapDialog 弹窗 PNG | `HeatmapView.*`, `HeatmapDialog.*` |

### 1.4 UI 与工程

| 模块 | 能力 | 说明 |
|------|------|------|
| 主窗口 | 1600×900、左右两栏工业布局、状态栏坐标/进度 | `MainWindow.*`（迁移前） |
| 构建 | CMake 3.20+、Qt 6.8.3、MSVC 2022、CI Release | `CMakeLists.txt`, `scripts/build_windows_msvc.ps1` |
| 样式 | app.qss 浅色工业风 | `resources/styles/app.qss` |

---

## 2. nfs-scanner-pro 可迁移内容清单

### 2.1 产品规范（文档层，不机械翻译代码）

| 类别 | 来源 | 可迁移价值 |
|------|------|------------|
| AI 入口与原则 | `spec/AI_INDEX.md` | 10 条不可违反产品原则 |
| 主窗口高保真 | `docs/product-spec/high-fidelity/main-window/` | 1920×1080 尺寸、四页、Dock、设备状态栏 |
| 线框 | `docs/product-spec/ui-wireframe/01_Main_Window_1920x1080.md` | 区域像素、菜单/工具栏结构 |
| Qt 规范 | `docs/product-spec/qt-spec/02_Qt_Object_Names.md` | objectName 命名 |
| 画布规则 | `Qt_GraphicsView_Rules.md` | 热力图必须整图 QPixmapItem，禁止逐格 Rect |
| Release 011~015 | Mock 页面结构与交互验收标准 | 四页壳层、设备卡片、扫描 Mock、分析/报告 Mock |
| 领域模型 | `docs/product-spec/domain/` | Project 文件夹、ScanTask 七态、AlignmentConfig |
| ADR | Project 非一级导航、Camera 可选、Hx/Hy 切换 | 架构决策约束 C++ 实现 |

### 2.2 Python Mock 实现（行为参考，非直译）

| Release | 参考文件 | C++ 应对方式 |
|---------|----------|--------------|
| 011 | `ui/main_window.py`, `navigation_bar.py` | MainWindow 四页 + 导航 + Dock |
| 012 | `ui/pages/device_page.py` | 设备页整合现有 Serial + Spectrum UI |
| 013 | `ui/scan_task_mock.py` | 对接已有 ScanManager 真实逻辑 |
| 014 | `ui/analysis_mock.py` | 对接 FrequencyCsvParser / HeatmapGenerator |
| 015 | `ui/report_mock.py` | 报告页占位 + TODO(report) |
| 016+ | project_mock, workspace_state | TODO(project) 文件夹容器 |

---

## 3. 差异表（已完成 / 部分完成 / 未完成）

| 能力域 | nfs-scanner-pro 目标 | nfs-scanner-cpp 现状 | 状态 |
|--------|---------------------|---------------------|------|
| 四页一级导航 | 扫描/设备/分析/报告 | 迁移前：左右两栏无导航 | **部分完成**（Release 010C 壳层） |
| Project 管理 | 文件菜单新建/打开/保存文件夹 | 仅有项目名称/测试名称输入框 | **未完成** |
| 右侧参数 Dock | 按页切换 QDockWidget | 迁移前：固定右栏 GroupBox | **部分完成** |
| 视图菜单隐藏面板 | 日志/频谱/统计/数据表默认隐藏 | 迁移前：日志默认可见 | **部分完成** |
| PCB 画布 QGraphicsView | Scene 七层 + QPixmapItem 热力图 | HeatmapView QWidget 整图绘制 | **部分完成**（原则满足，非 QGraphicsView） |
| 设备状态栏 | 四设备芯片 + 探头/区域/频率/点数 | 无独立设备状态栏 | **未完成** |
| 工具栏 | 开始/停止扫描、拍照、对齐等 | 扫描按钮在左侧 GroupBox | **部分完成** |
| 扫描状态机七态 | 含未就绪/停止中等 | ScanManager 八态（更细） | **已完成**（C++ 更强） |
| 真实运动控制 | Mock + 未来真实 | SerialMotionController 已实现 | **已完成** |
| 真实频谱仪 | Mock + 未来真实 | ZNA67/FSW/N9020A + Mock fallback | **已完成** |
| 任务持久化 | project 文件夹 + scan 子目录 | TaskStorage 单任务目录 | **部分完成** |
| 离线分析 | Trace/频率/LUT/导出 | FrequencyCsvParser + HeatmapGenerator | **已完成** |
| 相机 / Alignment | 可选，不影响扫描 | 无 | **未完成** |
| 舵机 Hx/Hy | 设备页 Mock | 无 | **未完成** |
| 报告 PDF/HTML | Release 015 Mock | 无 | **未完成** |
| 授权 | 未来离线机器绑定 | 无 | **未完成** |
| SCPI 设备线程 | 规范建议专用线程 | UI 线程创建 analyzer，采集在 worker 线程 | **部分完成** |
| Mock fallback | 无硬件可演示 | Mock Motion + Mock Spectrum 均已保留 | **已完成** |
| CI / 安装包 | verify.yml + 多 Release 验收 | Windows Build + Release tag 打安装包 | **部分完成** |

---

## 4. UI 迁移清单

### 4.1 MainWindow 结构差距（迁移前 → 目标）

| 区域 | 迁移前 (v0.9.0) | 目标 (Pro Release 011) | Release 010C 动作 |
|------|----------------|------------------------|-------------------|
| 菜单栏 | 无 | 文件/编辑/视图/工具/设置/帮助 | 新增文件+视图菜单 |
| 工具栏 | 无 | 开始/暂停/停止扫描 | 新增简化工具栏 |
| 左侧 | 串口+运动+命令+步长+测试+操作 | 64px 图标导航四页 | 新增 QListWidget 导航 |
| 中央 | 右栏堆叠 GroupBox | QStackedWidget 四页 | 扫描页=HeatmapView |
| 右侧 | 扫描区+仪表+结果+热力图+日志 | QDockWidget 按页切换 | paramDock + stacked |
| 底部 Dock | 无 | 日志/频谱/统计/数据表（默认隐藏） | 新增四个 QDockWidget |
| 状态栏 | 坐标/时间/剩余/预计/状态 | 同 + 扫描进度条 | 保留现有 |

### 4.2 Navigation

- [x] 仅四项：扫描、设备、分析、报告（objectName: `navScanButton` 等）
- [ ] Hover 64→180px 动画（后续 QSS/动画）
- [x] 禁止「项目」作为一级导航

### 4.3 Dock

- [x] `scanParamDock` 内容：扫描区域、测试说明、步长、开始/暂停/停止
- [x] `deviceConfigDock` 内容：仪表区域（频谱仪连接与配置）
- [x] `analysisParamDock` 内容：结果区 Trace/频率/LUT/Colorbar
- [x] `reportSettingsDock` 内容：占位
- [x] `logDock` / `spectrumDock` / `statisticsDock` / `dataTableDock` 默认隐藏

### 4.4 StatusBar

- [x] 保留 X/Y/Z、时间、剩余点数、预计完成、状态文本
- [ ] 独立进度条在状态栏（当前进度在扫描参数 Dock 内）

---

## 5. 设备页迁移清单

| 项 | Pro Mock (R012) | C++ 现状 | 迁移策略 |
|----|-----------------|----------|----------|
| 运动平台卡片 | 坐标/Jog/回零 | SerialMotionController UI 完整 | **已迁入设备页中央** |
| 频谱仪卡片 | 连接/Sweep/Trace | ISpectrumAnalyzer UI 完整 | **已迁入设备页 Dock** |
| 相机卡片 | 预览 Mock | 无 | TODO(camera) 占位 |
| 舵机卡片 | Hx/Hy 切换 | 无 | 占位标签 |
| 连接状态 | 四设备芯片状态栏 | deviceDiscoveryLabel 局部 | Release 012C 统一 DeviceStatusBar |
| 真实硬件 | Mock only in Pro | C++ 已有真实串口+SCPI | **保留，不破坏** |

---

## 6. 扫描页迁移清单

| 项 | Pro (R013) | C++ 现状 | 状态 |
|----|------------|----------|------|
| 扫描区域表格 | Dock 内区域设置 | scanTable_ 9 列 | **部分完成** |
| 蛇形路径 | checkbox | snakeModeCheck_ | **已完成** |
| 驻留时间 | spin | dwellTimeSpinBox_ | **已完成** |
| 实时进度 | 状态栏+画布高亮 | ScanManager signals + HeatmapView | **已完成** |
| 热力图叠加 | QGraphicsPixmapItem | HeatmapView QImage 整图 | **已完成**（实现路径不同） |
| 扫描中锁定参数 | Dock 只读 | 未实现 UI 锁定 | Release 011C |
| 与 ScanManager 整合 | Mock QTimer | 真实 ScanManager | **C++ 领先** |

---

## 7. 分析页迁移清单

| 项 | Pro (R014) | C++ 现状 | 状态 |
|----|------------|----------|------|
| 加载 traces.csv | Mock 路径 | loadFrequencyData() + FrequencyCsvParser | **已完成** |
| Trace 选择 | Combo | traceCombo_ | **已完成** |
| 频率选择 | Combo | frequencyCombo_ | **已完成** |
| 显示模式 | magnitude/db/phase/re/im | displayModeCombo_ | **已完成** |
| LUT | turbo 等 | LutManager + lutCombo_ | **已完成** |
| Colorbar | 画布右侧 | colorbarLabel_ 面板 | **部分完成** |
| 导出 PNG | Mock 状态栏 | HeatmapDialog 可导出 | **已完成** |
| 十字线/光标读数 | Mock 浮窗 | 无 | Release 013C |
| 扫描完成自动加载 | Mock 联动 | scanFinished → loadFrequencyData | **已完成** |

---

## 8. 报告页迁移清单

| 项 | Pro (R015) | C++ 现状 | 状态 |
|----|------------|----------|------|
| 报告列表 | 3 条 Mock | 无 | **未完成** |
| PDF 风预览 | 白色预览区 | 无 | TODO(report) |
| 报告设置 Dock | 模板/导出/内容 | 占位 Widget | **部分完成** |
| 导出 PDF/HTML | Mock QTimer | 无 | TODO(report) |
| 截图嵌入 | Mock 缩略图 | 可复用 HeatmapView QImage | Release 015C |

---

## 9. 数据模型迁移清单

| 对象/文件 | Pro 领域模型 | C++ 现状 | 差距 |
|-----------|-------------|----------|------|
| Project 文件夹 | ADR-0018 根目录容器 | 无统一 Project 类 | TODO(project) |
| scan_config | scan_config.json | ScanConfig → scan_config.json | **已完成** |
| points.csv | 扫描点序列 | TaskStorage.appendPoint | **已完成** |
| traces.csv | 多 trace re/im | TaskStorage.appendTrace | **已完成** |
| meta.json | 任务元数据 | TaskStorage.writeMetaJson | **已完成** |
| AlignmentConfig | Region 对齐、Hx/Hy 补偿 | 无 | TODO(camera) + 领域占位 |
| workspace 持久化 | Release 017 | 无 | 低优先级 |
| project.json | Release 016 Mock | 无 | TODO(project) |

---

## 10. 风险清单

| 风险 | 描述 | 缓解措施 |
|------|------|----------|
| 真实硬件线程 | SCPI analyzer 对象在 UI 线程创建/连接，采集虽在 worker 但 connect/configure 可能阻塞 UI | TODO(device-thread)；短期保留 timeout + 异步 worker |
| SCPI 超时 | 网络抖动导致采集失败 | 已有 timeout/retry/stopOnError；ScanManager 错误态停止 |
| UI 卡顿 | MainWindow 2200+ 行，重构易引入回归 | 小步迁移、每步构建验证、保留原有 slot 逻辑 |
| 相机依赖 | OpenCV/USB 相机引入构建复杂度 | Camera 可选模块，接口 + TODO，不阻塞扫描 |
| OpenCV 引入 | 对齐算法可能需要 cv:: | 仅接口占位，Release 014C 再评估 |
| 授权模块 | 离线机器绑定、安装包签名 | TODO(license)，Release 015C |
| 热力图性能 | 禁止逐格 Rect，大网格 QImage 内存 | 已用 HeatmapGenerator 整图；注意尺寸上限 |
| Mock fallback 丢失 | 重构 UI 时断开 Mock 路径 | 保留 mockModeCheck_、Mock Spectrum 自动 fallback |
| 双仓库规范漂移 | Pro spec 更新后 C++ 未同步 | 本文档 + Registry 索引定期对齐 |

---

## 11. 推荐里程碑

### Release 010C — C++ UI 壳层对齐（当前阶段）

- [x] 迁移审计文档
- [x] MainWindow 四页 QStackedWidget + 左侧导航
- [x] 文件/视图菜单 + 参数 Dock 按页切换
- [x] 日志/频谱/统计/数据表 Dock 默认隐藏
- [x] 保留 ScanManager / 运动 / 频谱仪全部入口
- [x] CMake 构建通过

### Release 011C — 扫描页与真实扫描逻辑整合

- 扫描页画布与 ScanManager 进度/热力图实时联动
- 扫描中锁定参数 Dock
- 工具栏与扫描态 enabled/disabled 联动
- 设备就绪 gate（未连接时提示，仍允许 Mock）

### Release 012C — 设备页整合真实设备管理

- DeviceStatusBar 四设备芯片
- 设备页卡片式布局（运动/频谱/相机占位/舵机占位）
- TODO(device-thread)：SCPI 迁移到专用 QThread
- 频谱仪 Tab 去除「预留页」占位，FSW/N9020A 完整 UI

### Release 013C — 分析页增强

- 分析页独立画布或 QGraphicsView 层
- 十字线、光标读数、频谱 Dock 静态曲线
- LUT/Colorbar 与高保真布局对齐
- 参数变更实时刷新热力图（debounce）

### Release 014C — 相机与 Alignment

- TODO(camera)：ICamera 接口 + MockCamera
- AlignmentConfig 数据结构
- Hx/Hy 切换与偏移补偿 UI（不接 OpenCV 或仅接口）

### Release 015C — 报告与打包授权

- TODO(report)：报告模板、HTML/PDF 导出管道
- TODO(license)：离线授权校验
- 安装包与 verify 脚本对齐 Pro Release 023+

---

## 12. MainWindow 结构差距说明（Release 010C 实施后）

### 迁移前（v0.9.0）

```text
┌─────────────────────────────────────────────────────────┐
│ MainWindow (无菜单栏)                                    │
├──────────────────┬──────────────────────────────────────┤
│ 左栏              │ 右栏                                  │
│ 串口/运动/命令    │ 扫描区域 / 仪表 / 结果 / 热力图 / 日志 │
│ 步长/测试/操作    │                                       │
├──────────────────┴──────────────────────────────────────┤
│ QStatusBar                                               │
└─────────────────────────────────────────────────────────┘
```

### Release 010C 目标结构

```text
┌─────────────────────────────────────────────────────────┐
│ 菜单栏：文件 | 编辑 | 视图 | 工具 | 设置 | 帮助          │
├─────────────────────────────────────────────────────────┤
│ 工具栏：开始 | 暂停 | 停止                               │
├────┬──────────────────────────────────────────┬─────────┤
│导航│  QStackedWidget                          │ param   │
│扫描│  [0] HeatmapView 扫描画布                │ Dock    │
│设备│  [1] 运动/串口控件                       │ (按页   │
│分析│  [2] 分析预览占位                        │ 切换)   │
│报告│  [3] 报告占位                            │         │
├────┴──────────────────────────────────────────┴─────────┤
│ QStatusBar（坐标/剩余/预计/状态）                        │
└─────────────────────────────────────────────────────────┘
  隐藏 Dock：日志 | 频谱 | 统计 | 数据表格（视图菜单打开）
```

### 功能归属映射

| 原 GroupBox | 新位置 |
|-------------|--------|
| 串口设置、运动控制、运动命令 | 设备页中央 |
| 步长设置、测试说明、功能操作区 | 扫描参数 Dock |
| 扫描区域 | 扫描参数 Dock |
| 仪表区域 | 设备参数 Dock |
| 结果区域 | 分析参数 Dock |
| 热力图预览 | 扫描页中央 (HeatmapView) |
| 日志区域 | 日志 Dock（默认隐藏） |

---

## 13. 构建验证记录

> 更新日期：2026-06-30（Release 010C）

### 构建命令

```powershell
# 首次尝试（失败：cmake 未安装）
powershell -ExecutionPolicy Bypass -File scripts/build_windows_msvc.ps1

# 环境准备
powershell -ExecutionPolicy Bypass -File scripts/setup_qt_windows.ps1   # 成功安装 Qt 6.8.3
python -m pip install cmake                                            # 成功，cmake 4.3.2

# 第二次尝试（使用 pip cmake）
powershell -ExecutionPolicy Bypass -File scripts/build_windows_msvc.ps1 `
  -CMakePath "C:/Users/yunfei/AppData/Roaming/Python/Python313/site-packages/cmake/data/bin/cmake.exe"
```

### 构建结果

**Visual Studio 2022 Build Tools**：✅ 已安装  
**NFSScanner.exe**：✅ `build/Release/NFSScanner.exe`  
**NFSScannerSelfCheck.exe**：✅ 全部 15 项 PASS（需 Qt bin 在 PATH）

```powershell
$env:PATH = "C:/Qt/6.8.3/msvc2022_64/bin;" + $env:PATH
.\build\Release\NFSScannerSelfCheck.exe
.\build\Release\NFSScanner.exe
```

### 错误与修复

1. **cmake 未在 PATH**  
   - 修复：运行 `python -m pip install cmake`，构建时传入 `-CMakePath` 指向 pip 安装的 `cmake.exe`。

2. **Visual Studio 2022 未安装**  
   - 阻塞项：需安装 [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/downloads/) 并勾选 **「使用 C++ 的桌面开发」** 工作负载。  
   - 安装完成后重新运行：

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_windows_msvc.ps1 `
  -CMakePath "C:/Users/yunfei/AppData/Roaming/Python/Python313/site-packages/cmake/data/bin/cmake.exe"
```

3. **代码层面**  
   - Release 010C MainWindow 重构已完成，待 VS 工具链就绪后做首次编译验证。

### 预期下一步（构建环境就绪后）

- 确认 `build/Release/NFSScanner.exe` 生成
- 手动验证：四页导航切换、视图菜单显示/隐藏 Dock、扫描开始/暂停/停止、Mock 模式演示

---

*文档维护：每完成一个 Release 0xxC 里程碑，更新第 3 节差异表与第 13 节构建记录。*
