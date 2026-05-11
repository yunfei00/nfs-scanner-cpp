# 数据存储说明

## v0.4.0 数据存储范围

v0.4.0 在扫描开始时创建任务目录，并在扫描过程中持续写入扫描点和 Mock 频谱数据。当前阶段不接真实频谱仪，不做离线加载，不生成热力图，也不使用 SQLite。

## 任务目录结构

默认根目录为 `data/scans`。如果界面结果区域填写了其他目录，则使用该目录作为扫描根目录。

```text
data/
  scans/
    yyyyMMdd_HHmmss_project_test/
      meta.json
      scan_config.json
      points.csv
      traces.csv
      exports/
```

任务目录中的 `project` 和 `test` 来自界面“测试说明”，非法文件名字符会被移除，空格会替换为 `_`。

## meta.json 字段

- `app_name`：应用名称。
- `app_version`：应用版本。
- `data_format_version`：数据格式版本，v0.4.0 为 `0.2`。
- `created_at`：任务创建时间。
- `project_name`：项目名称。
- `test_name`：测试名称。
- `point_count`：扫描路径点数。
- `task_dir`：任务目录绝对路径。

## scan_config.json 字段

- `startX/startY/startZ`：扫描起点。
- `endX/endY/endZ`：扫描终点。
- `stepX/stepY/stepZ`：扫描步长。
- `feed`：运动速度预留字段。
- `dwellMs`：单点驻留时间。
- `snakeMode`：是否启用蛇形扫描。
- `projectName/testName/outputDir`：任务描述与输出目录。

## points.csv 格式

第一行固定为：

```text
index,x,y,z,timestamp
```

后续每个扫描点追加一行，坐标保留 3 位小数，时间使用 ISO 格式。

## traces.csv 格式

第一行写频率轴：

```text
fre,1000000,5995000,...
```

每个扫描点追加两行：

```text
x_y_z_Trc1_S21_re,value1,value2,value3,...
x_y_z_Trc1_S21_im,0,0,0,...
```

当前 Mock 数据只写实部幅度，虚部 `im` 固定为 0。

## Python 格式兼容

`traces.csv` 采用旧 Python 版频率文件的行式结构：首行为 `fre` 频率轴，后续按点位和 trace id 写 `_re` / `_im` 两行。这样后续热力图和旧工具适配时可以复用同类解析方式。

## 后续频谱仪接入计划

后续计划将 `MockSpectrumAnalyzer` 替换为真实频谱仪适配器，保持 `SpectrumTrace` 输出结构稳定。

## v0.6.0 离线加载和热力图计划

后续将增加任务目录离线加载、扫描数据校验、热力图生成和结果导出。
