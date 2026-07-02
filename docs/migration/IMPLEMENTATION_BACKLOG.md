# NFS Scanner C++ 实现 Backlog

> 分支：`feature/full-python-pro-migration`  
> 最后更新：2026-07-02

## v0.11.0-hardware-alpha（真实硬件预实现）

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| HW-1 | HardwareConfig + JSON 加载/保存 | [x] | `config/hardware_config.json` |
| HW-2 | DeviceManager 统一 connect API | [x] | motion/spectrum/camera/probe |
| HW-3 | PreScanChecklist | [x] | 扫描开始前 UI 对话框 |
| HW-4 | HardwareDiagnostics 导出 | [x] | Help → 诊断信息 |
| HW-5 | 探头 Hx/Hy + scan_config | [x] | Mock + Stub |
| HW-6 | SelfCheck 扩展 | [x] | 84/84 PASS |
| HW-7 | 硬件联调文档 | [x] | `docs/hardware/*` |
| HW-8 | GRBL/SCPI 现场验证 | [!] | 需真实硬件 |

状态：`[ ]` 待办 · `[~]` 进行中 · `[x]` 完成 · `[!]` 需人工验证

---

## P0 构建与基础稳定性

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| P0-1 | Qt 6.8.3 + CMake 环境 | [x] | Qt 已安装；CMake 通过 pip |
| P0-2 | MSVC 2022 工具链 | [x] | winget Build Tools |
| P0-3 | Release 构建通过 | [x] | 2026-06-30 验证 |
| P0-4 | 版本号升至 v0.10.0 | [x] | AppVersion.h |
| P0-5 | CMakeLists 新模块注册 | [x] | UiFormUtils / pages / DeviceStatusBar |

## P1 主窗口与四页导航

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| P1-1 | 四页 QStackedWidget | [x] | Release 010C |
| P1-2 | 左侧导航四项 | [x] | 无 Project 页 |
| P1-3 | 文件/视图/设备/扫描/工具/帮助菜单 | [x] | |
| P1-4 | 独立页面类 Scan/Device/Analysis/Report | [x] | src/ui/pages/ |
| P1-5 | MainWindow 仅组合页面 | [x] | ~880 行 cpp，业务迁入各 Page |
| P1-6 | 参数 Dock 按页切换 | [x] | |
| P1-7 | 日志/频谱/统计/数据表默认隐藏 | [x] | |

## P2 设备管理页

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| P2-1 | DeviceManager 统一设备 | [x] | core/DeviceManager |
| P2-2 | ICamera + MockCamera | [x] | 可选，不影响扫描 |
| P2-3 | 运动平台卡片 UI | [x] | 在 DevicePage |
| P2-4 | 频谱仪卡片 UI | [x] | 保留 ZNA67/FSW/N9020A |
| P2-5 | 相机卡片占位 | [~] | DevicePage + Mock 截图接线 |
| P2-6 | 系统诊断卡片 | [~] | showDiagnosticsDialog |
| P2-7 | 设备菜单 刷新/连接全部/断开全部 | [x] | |
| P2-8 | SCPI 设备线程化 | [x] | SpectrumDeviceHost + 共享 QThread |
| P2-9 | DeviceStatusBar 六芯片 | [x] | 运动/频谱/相机/项目/授权/扫描 |

## P3 扫描页

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| P3-1 | ScanManager 接入（不重写） | [x] | |
| P3-2 | 扫描参数区完整 | [x] | scanTable + snake + dwell |
| P3-3 | 开始/暂停/继续/停止 | [x] | |
| P3-4 | HeatmapView 中央画布 | [x] | 整图 QImage |
| P3-5 | 路径预览 | [x] | ScanPathPlanner + HeatmapView overlay |
| P3-6 | 扫描中锁定参数 | [x] | setScanParamsLocked |
| P3-7 | alignment.json 写入 | [x] | saveAlignmentForTaskDir |
| P3-8 | Mock fallback 保留 | [x] | |

## P4 分析页

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| P4-1 | traces.csv 加载 | [x] | FrequencyCsvParser |
| P4-2 | Trace/频率/显示模式 | [x] | |
| P4-3 | LUT 全列表 | [x] | LutManager |
| P4-4 | vmin/vmax/透明度/Colorbar | [x] | |
| P4-5 | PNG 导出 | [x] | HeatmapDialog |
| P4-6 | 分析配置 JSON 导出 | [~] | AnalysisPage |
| P4-7 | 十字线/光标读数 | [x] | HeatmapView crosshair + hintLabel |
| P4-8 | 实时参数刷新热力图 | [x] | 300ms debounce refreshHeatmapPreview |

## P5 热力图与 Alignment

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| P5-1 | AlignmentConfig 数据结构 | [x] | |
| P5-2 | alignment.json 读写 | [x] | self_check 往返测试 |
| P5-3 | 世界坐标→像素线性映射 | [x] | 无 OpenCV |
| P5-4 | AlignmentEditor UI | [x] | 扫描 Dock + Mock 相机截图 |
| P5-5 | 热力图叠加背景 | [x] | HeatmapView setBackgroundImage |
| P5-6 | 无 Alignment 仍可扫描 | [x] | 设计原则 |
| P5-7 | 四点透视标定 | [x] | QTransform::quadToQuad，无 OpenCV |

## P6 相机模块

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| P6-1 | ICamera 接口 | [~] | |
| P6-2 | MockCamera 预览帧 | [x] | Mock 截图 → AlignmentEditor |
| P6-3 | USB/工业相机占位 | [~] | 不引 OpenCV |
| P6-4 | Capture 不影响 ScanManager | [x] | 设计原则 |

## P7 项目文件夹与数据管理

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| P7-1 | Project / ProjectManager | [x] | |
| P7-2 | project.json | [x] | |
| P7-3 | 新建/打开/保存/另存为 | [x] | 文件菜单 |
| P7-4 | 最近项目 | [x] | QSettings |
| P7-5 | 扫描保存到 project/scans/ | [x] | ScanPage + defaultScanOutputDir |
| P7-6 | 无项目时用 workspace | [x] | workspace/scans + reports |
| P7-7 | 状态栏显示当前项目 | [x] | DeviceStatusBar 项目芯片 |

## P8 报告模块

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| P8-1 | ReportData 模型 | [x] | |
| P8-2 | ReportGenerator HTML/MD/PDF | [x] | Qt PrintSupport |
| P8-3 | ReportPage 列表+预览 | [x] | |
| P8-4 | PNG 图片集合导出 | [x] | |
| P8-5 | PDF 导出 | [x] | QPdfWriter |
| P8-6 | 输出到 project/reports 或 workspace/reports | [x] | ReportPage |
| P8-7 | self_check 报告导出 | [x] | MD/HTML 临时目录 |

## P9 授权模块

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| P9-1 | MachineId 生成 | [x] | |
| P9-2 | LicenseManager + Demo 校验 | [x] | machine_id + 过期 |
| P9-3 | license.json 读取 | [x] | 不提交真实 license |
| P9-4 | UI 授权状态 | [x] | DeviceStatusBar 授权芯片 |
| P9-5 | Demo 模式功能限制 | [x] | 软限制 scan/analysis/report |
| P9-6 | Ed25519 非对称签名 | [x] | LicenseSignatureVerifier；厂商私钥需人工签发 |

## P10 打包与发布

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| P10-1 | build_windows_msvc.ps1 | [x] | 2026-06-30 Release 通过 |
| P10-2 | package_portable | [x] | v0.10.0-alpha 2026-07-02 复验 ~22MB |
| P10-3 | build_installer | [x] | ISCC: `%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe` |
| P10-4 | GitHub Actions | [x] | 已有 |
| P10-5 | README 更新 v0.10.0 | [x] | |
| P10-6 | CI 构建验证 | [x] | 本地 Release + portable 验证 |

## P11 自动化测试与自检

| ID | 任务 | 状态 | 备注 |
|----|------|------|------|
| P11-1 | self_check 命令行工具 | [x] | 47 项 PASS |
| P11-2 | 蛇形路径测试 | [x] | |
| P11-3 | traces.csv 解析测试 | [x] | |
| P11-4 | LUT 测试 | [x] | |
| P11-5 | Alignment 映射 + JSON + 透视 | [x] | |
| P11-6 | Project 创建测试 | [x] | |
| P11-7 | SELF_TEST_CHECKLIST 更新 | [x] | |

---

## 优先级执行顺序（自动循环）

1. P0 构建通过  
2. P1 页面类 + 完整菜单  
3. P2 DeviceManager + DevicePage  
4. P7 ProjectManager 接入扫描路径  
5. P3 ScanPage 增强  
6. P4 AnalysisPage 增强  
7. P5 Alignment  
8. P8 Report  
9. P9 License  
10. P11 self_check  
11. P10 README + 版本  

---

## 后续剩余 TODO（Manual verification required）

- Alignment 矩形拖拽 / HeatmapView 悬停世界坐标 GUI 手测
- 厂商 Ed25519 私钥签发正式 license（见 LICENSE_SIGNING.md）
- 真实硬件：GRBL / ZNA67 / FSW / N9020A / USB 相机 / 舵机 Hx/Hy
- GUI 完整扫描流程与报告 PDF 导出手测

## 下一步建议（v0.10.0-alpha 稳定节点）

1. 分支已推送：`feature/full-python-pro-migration` → 创建 PR 合并 main
2. Mock UI 人工走查（四页切换、Mock 扫描、报告导出）
3. 单硬件逐项联调（建议先 GRBL 或 ZNA67）
4. 验证通过后打 tag：`git tag v0.10.0-alpha && git push origin v0.10.0-alpha`
