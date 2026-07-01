# 离线 License 签名（需人工 / 厂商私钥）

应用内仅嵌入 **Ed25519 公钥**（`LicenseSignatureVerifier::embeddedPublicKey()`）。  
**私钥不得提交到仓库。**

## 开发环境测试密钥

开发公钥由公开 seed 字符串 `NFSScannerLicenseDevSeed2026` 派生，仅用于 Demo / self_check。  
正式发布前应替换为厂商独立密钥对，并仅更新应用内公钥字节。

## 签名步骤（需安装 libsodium 或 OpenSSL ed25519 工具）

1. 准备 `license.json`（不含 `signature` / `signature_alg` 字段）。
2. 使用厂商私钥对 **canonical JSON**（与 `LicenseSignatureVerifier::buildCanonicalPayload` 相同：去掉 signature 字段后的 compact JSON）签名。
3. 将 Base64 签名写入：

```json
{
  "license_id": "...",
  "machine_id": "...",
  "expire_date": "2027-12-31",
  "features": ["scan", "analysis", "report"],
  "signature_alg": "ed25519",
  "signature": "<base64>"
}
```

4. 放置到 `%APPDATA%/NFSScanner/license/license.json`（或应用配置目录）。

## 验证

- `signature` 非空且 `signature_alg=ed25519` 时，必须通过 Ed25519 校验才为 **已授权**。
- `signature` 为空时按 Demo 规则（machine_id 绑定，无强签名）。

## 人工验证项

- [ ] 厂商生产密钥对生成与保管
- [ ] 替换应用内 embedded 公钥
- [ ] 现场签发真实 license.json
