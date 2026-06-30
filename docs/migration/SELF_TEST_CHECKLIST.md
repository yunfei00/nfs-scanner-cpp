# NFS Scanner C++ 自测清单

> 分支：`feature/full-python-pro-migration`  
> 执行方式：构建后运行 `NFSScanner.exe` 或 `NFSScannerSelfCheck.exe`

---

## 构建测试

| # | 项 | 命令 | 结果 | 日期 |
|---|-----|------|------|------|
| B1 | Windows MSVC Release | `scripts/build_windows_msvc.ps1` | ✅ 2026-06-30 | 2026-06-30 |
| B2 | CMake 配置 | VS 2022 Build Tools + Qt 6.8.3 | ✅ | 2026-06-30 |
| B3 | self_check 构建 | `NFSScannerSelfCheck.exe` | ✅ | 2026-06-30 |

**环境记录：**

- Qt 6.8.3 msvc2022_64：✅ `C:/Qt/6.8.3/msvc2022_64`
- CMake 4.3.2 (pip)：✅
- Visual Studio 2022：⏳ winget 安装中

---

## 启动测试

| # | 项 | 预期 | 结果 |
|---|-----|------|------|
| S1 | 应用启动无崩溃 | 主窗口 1600×900 | ⏳ |
| S2 | 默认模拟模式 | 日志提示 Mock | ⏳ |
| S3 | 默认扫描页 | 导航第一项选中 | ⏳ |

---

## 主窗口测试

| # | 项 | 预期 | 结果 |
|---|-----|------|------|
| M1 | 左侧仅四页导航 | 扫描/设备/分析/报告 | ⏳ |
| M2 | 无 Project 一级导航 | | ⏳ |
| M3 | 文件菜单项目操作 | 新建/打开/保存 | ⏳ |
| M4 | 视图菜单隐藏 Dock | 日志默认不可见 | ⏳ |
| M5 | 参数 Dock 随页切换 | 标题变化 | ⏳ |
| M6 | 状态栏坐标/剩余/预计 | 实时更新 | ⏳ |

---

## 页面切换测试

| # | 项 | 预期 | 结果 |
|---|-----|------|------|
| P1 | 扫描→设备→分析→报告 | 无崩溃 | ⏳ |
| P2 | Dock 内容联动 | 四套参数面板 | ⏳ |

---

## Mock 运动测试

| # | 项 | 预期 | 结果 |
|---|-----|------|------|
| MO1 | 模拟模式点动 X/Y/Z | 坐标变化 | ⏳ |
| MO2 | 模拟复位 | 坐标归零 | ⏳ |
| MO3 | 坐标越界拒绝 | 日志提示 | ⏳ |

---

## Mock 频谱测试

| # | 项 | 预期 | 结果 |
|---|-----|------|------|
| SP1 | Mock Spectrum 连接 | 成功 | ⏳ |
| SP2 | 单次扫描 | 返回 trace | ⏳ |
| SP3 | 未连接真实仪表扫描 | Mock fallback | ⏳ |

---

## 扫描路径测试（self_check）

| # | 项 | 预期 | 结果 |
|---|-----|------|------|
| SC1 | 蛇形路径偶数行反转 | self_check PASS | ⏳ |
| SC2 | stepX/stepY > 0 校验 | 错误时空路径 | ⏳ |
| SC3 | 2×2 网格 4 点 | index 1..4 | ⏳ |

---

## 数据保存测试

| # | 项 | 预期 | 结果 |
|---|-----|------|------|
| D1 | 扫描生成 task 目录 | meta/scan_config/points/traces | ⏳ |
| D2 | traces.csv 多 trace 格式 | 兼容 Python | ⏳ |
| D3 | 项目 scans/ 子目录 | ProjectManager 路径 | ⏳ |

---

## traces.csv 解析测试（self_check）

| # | 项 | 预期 | 结果 |
|---|-----|------|------|
| F1 | 格式 A 加载 | FrequencyData valid | ⏳ |
| F2 | trace_id 自动发现 | traceCombo 填充 | ⏳ |
| F3 | 频率索引 | freqIndex 正确 | ⏳ |

---

## 热力图生成测试

| # | 项 | 预期 | 结果 |
|---|-----|------|------|
| H1 | magnitude 模式 | QImage 非空 | ⏳ |
| H2 | 整图绘制 | 无 QGraphicsRectItem 逐格 | ⏳ |
| H3 | 扫描进度 Mock 热力 | HeatmapView 更新 | ⏳ |

---

## LUT / Colorbar 测试（self_check）

| # | 项 | 预期 | 结果 |
|---|-----|------|------|
| L1 | 11+ LUT 名称 | availableLuts 非空 | ⏳ |
| L2 | colorbar 尺寸 | 28×120 | ⏳ |
| L3 | turbo/viridis 采样 | t=0/1 不同 | ⏳ |

---

## PNG 导出测试

| # | 项 | 预期 | 结果 |
|---|-----|------|------|
| E1 | HeatmapDialog 导出 | 文件可写 | ⏳ |
| E2 | 报告 PNG 集合 | ReportGenerator | ⏳ |

---

## 报告页面测试

| # | 项 | 预期 | 结果 |
|---|-----|------|------|
| R1 | 报告列表 Mock | 可点击 | ⏳ |
| R2 | HTML 导出 | 文件生成 | ⏳ |
| R3 | Markdown 导出 | 文件生成 | ⏳ |

---

## 授权状态测试

| # | 项 | 预期 | 结果 |
|---|-----|------|------|
| LI1 | machine_id 生成 | 非空稳定 | ⏳ |
| LI2 | 无 license Demo 模式 | 允许运行 | ⏳ |
| LI3 | 诊断卡片显示状态 | Demo/Licensed | ⏳ |

---

## 打包脚本测试

| # | 项 | 预期 | 结果 |
|---|-----|------|------|
| PK1 | package_portable_windows.ps1 | zip 生成 | ⏳ 需 B1 |
| PK2 | build_installer_windows.ps1 | exe 存在 | ⏳ |

---

## 真实硬件待人工验证项

| # | 项 | 说明 |
|---|-----|------|
| HW1 | GRBL 串口运动 | 需 COM 口 + 运动平台 |
| HW2 | ZNA67 TCP SCPI | 需仪表 IP |
| HW3 | FSW TCP SCPI | 需仪表 IP |
| HW4 | N9020A TCP SCPI | 需仪表 IP |
| HW5 | USB 相机 | 需相机硬件 + 未来驱动 |
| HW6 | 舵机 Hx/Hy | 需舵机控制器 |

---

## self_check 自动化命令

```powershell
.\build\Release\NFSScannerSelfCheck.exe
```

预期输出：`All self-check tests passed.` 或非零退出码列出失败项。
