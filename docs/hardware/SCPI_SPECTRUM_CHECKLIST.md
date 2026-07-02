# SCPI 频谱仪人工验证清单

支持：**R&S ZNA67**、**R&S FSW**、**Keysight N9020A**、**Generic SCPI**（TCP 5025 fallback）。

## 软件配置

```json
"spectrum": {
  "enabled": true,
  "type": "zna67",
  "address": "192.168.0.10",
  "port": 5025,
  "timeout_ms": 5000,
  "retry_count": 2
}
```

`type` 可选：`mock` | `zna67` | `fsw` | `n9020a` | `generic`

## 网络检查

- [ ] Ping 仪表 IP
- [ ] TCP **5025** 可连接（防火墙放行）
- [ ] 设备页或单项测试 → **连接频谱仪**
- [ ] **查询 IDN**（`*IDN?`）— 记录完整响应

## 参数与单次采集

- [ ] 应用配置（起止频率、RBW、点数、trace 名）
- [ ] **单次扫描**（worker 线程，不阻塞 UI）
- [ ] 保存单次扫描 CSV / 检查 traces 行
- [ ] ZNA67：Trc1_S21 / 复数 re/im
- [ ] FSW / N9020A：频率 + 幅度轴

## 扫描中采集

- [ ] 真实模式：Checklist 频谱项必须通过
- [ ] 每点采集失败可重试（`retry_count`）
- [ ] `stopOnError` 行为符合预期
- [ ] 结果写入 `traces.csv`

## 断线重连

- [ ] 拔网线 / 关仪表 → 明确错误提示
- [ ] 恢复网络 → 断开再连接 → IDN 成功
- [ ] 继续扫描或重新开始任务

## 常见错误

| 现象 | 处理 |
|------|------|
| 连接超时 | 增大 `timeout_ms`，检查 IP/端口 |
| IDN 空 | 确认 SCPI over TCP 已启用 |
| Trace 空 | 检查 trace 名称与 sweep 完成 |
| 频率不一致 | 重新 configure 后再 sweep |

## 回退 Mock

`spectrum.type: mock` 或设备页选择 Mock Spectrum。

## 需记录

IP、IDN、单次 trace 点数、扫描首点 trace 文件名、最近 SCPI 错误摘要（UI 日志，非全量 trace 数据）。
