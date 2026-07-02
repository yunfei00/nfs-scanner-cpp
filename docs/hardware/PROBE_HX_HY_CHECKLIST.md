# 探头 Hx / Hy 人工验证清单

## 软件配置

```json
"probe": {
  "enabled": true,
  "type": "mock",
  "orientation": "Hx",
  "switch_delay_ms": 500
}
```

扫描页 **测试说明 → 探头方向** 可选 Hx / Hy。

## Mock 验证

- [ ] 连接探头（Mock）
- [ ] 切换 **Hx**
- [ ] 切换 **Hy** — 等待 `switch_delay_ms`
- [ ] 扫描前 Checklist 探头项通过

## 数据记录

- [ ] `scan_config.json` 含 `probe_orientation`
- [ ] `meta.json` 含 `probe_orientation`
- [ ] 报告 Markdown 显示探头方向

## 真实控制（Stub）

| type | 状态 |
|------|------|
| serial | SerialProbeControllerStub — 待接线确认 |
| sdk | SdkProbeControllerStub — 待厂商 SDK |

## 扫描流程

- [ ] 若 `probe.enabled` 且非 Mock，必须 connected
- [ ] 每任务开始前 `setOrientation` 与 UI 一致

## 回退 Mock

`probe.type: mock`

## 需记录

Hx/Hy 切换耗时、舵机/继电器接线方式（待现场确认）、扫描任务 probe_orientation 字段。
