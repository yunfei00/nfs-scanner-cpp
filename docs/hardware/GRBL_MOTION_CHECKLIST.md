# GRBL 运动平台人工验证清单

## 软件配置

编辑 `config/hardware_config.json`：

```json
"motion": {
  "enabled": true,
  "type": "grbl",
  "port": "COM3",
  "baudrate": 115200,
  "limits": { "x_min": 0, "x_max": 200, "y_min": -300, "y_max": 0, "z_min": 0, "z_max": 10 }
}
```

**Y 轴范围：[-300, 0]**，**Z 轴：[0, 10]**。

## 单项连接测试

- [ ] 设备页 → 刷新串口
- [ ] 识别正确 COM 口（设备管理器对照）
- [ ] 波特率 **115200**
- [ ] 点击「连接运动平台」或打开串口
- [ ] 日志出现连接成功，无超时

## 单项动作测试

- [ ] 发送 `$I`（readVersion）— 记录固件信息
- [ ] 发送 `?`（queryPosition）— 解析 `MPos`
- [ ] **小步点动** X+/X-、Y+/Y-、Z+/Z-（±1 mm 量级）
- [ ] `$H` Home — **仅在安全、有人看管时**
- [ ] 移动到指定坐标（绝对 G1，带 F）
- [ ] 急停 / 停止 / Feed Hold
- [ ] 验证坐标限位：超范围命令应被拒绝并提示

## 扫描单点移动

- [ ] Mock 模式关闭
- [ ] 扫描前 Checklist 运动项为「已连接」
- [ ] 单点扫描：moveAbs → waitUntilIdle → dwell → 频谱采集

## 常见错误

| 现象 | 处理 |
|------|------|
| 串口占用 | 关闭其他终端/上位机 |
| ALARM | 发送 `$X` 解锁（若策略允许） |
| 无 MPos | 检查 `?` 响应格式 |
| 超限位 | 修正 scan 起止坐标或 limits |

## 回退 Mock

设备页勾选「模拟模式」，或 `motion.enabled=false`。

## 需记录

COM 口、$I 输出、Home 后 MPos、点动后 MPos、扫描首点坐标、失败日志摘要。
