# 相机人工验证清单

## 类型

| type | 状态 |
|------|------|
| mock | 已实现 — PCB 风格预览图 |
| usb | Stub — 需 OpenCV / 厂商 SDK |
| industrial | Stub — 需厂商 SDK |

## 软件配置

```json
"camera": {
  "enabled": true,
  "type": "mock",
  "save_dir": "images",
  "timeout_ms": 3000
}
```

## 单项测试（设备页）

- [ ] **连接相机**（Mock 无需 enabled 亦可手动测：`connectCamera(false)` 路径）
- [ ] **拍照** — 预览非空
- [ ] **保存图片** — `images/capture_*.png`
- [ ] **设置为 Alignment 背景** — Alignment 编辑器 Mock 捕获

## Alignment

- [ ] 背景图加载正常
- [ ] 透视/线性标定叠加正确
- [ ] 无相机时不阻止扫描（仅 Warning）

## 设备枚举（USB 待接入）

- [ ] 列出 USB 设备索引
- [ ] 选择 `device_index`
- [ ] 首帧延迟与 timeout

## 回退 Mock

`camera.type: mock` 或 `camera.enabled: false`。

## 需记录

保存路径、分辨率、Alignment 截图路径、失败 `lastError()`。
