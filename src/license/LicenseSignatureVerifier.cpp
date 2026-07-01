#include "license/LicenseSignatureVerifier.h"

extern "C" {
#include "license/ed25519/ed25519.h"
}

#include <QJsonDocument>

namespace NFSScanner::License {

namespace {

QString gLastError;
QByteArray gTestPublicKeyOverride;

// Public key bytes only. Replace with vendor production key before release.
// Dev key is derived from a public seed string; private signing key stays outside the repo.
static QByteArray devPublicKeyFromSeed()
{
    unsigned char seed[32];
    const char *text = "NFSScannerLicenseDevSeed2026";
    for (int i = 0; i < 32; ++i) {
        seed[i] = static_cast<unsigned char>(text[i % 27]);
    }
    unsigned char pk[32];
    unsigned char sk[64];
    ed25519_create_keypair(pk, sk, seed);
    Q_UNUSED(sk)
    return QByteArray(reinterpret_cast<const char *>(pk), 32);
}

} // namespace

QByteArray LicenseSignatureVerifier::buildCanonicalPayload(const QJsonObject &licenseObject)
{
    QJsonObject payload = licenseObject;
    payload.remove(QStringLiteral("signature"));
    payload.remove(QStringLiteral("signature_alg"));
    return QJsonDocument(payload).toJson(QJsonDocument::Compact);
}

QByteArray LicenseSignatureVerifier::embeddedPublicKey()
{
    static const QByteArray cached = devPublicKeyFromSeed();
    return cached;
}

void LicenseSignatureVerifier::setTestPublicKeyOverride(const QByteArray &publicKey32)
{
    gTestPublicKeyOverride = publicKey32;
}

void LicenseSignatureVerifier::clearTestPublicKeyOverride()
{
    gTestPublicKeyOverride.clear();
}

QByteArray LicenseSignatureVerifier::activePublicKey()
{
    if (gTestPublicKeyOverride.size() == 32) {
        return gTestPublicKeyOverride;
    }
    return embeddedPublicKey();
}

bool LicenseSignatureVerifier::verifyEd25519(const QByteArray &payload,
                                             const QByteArray &signatureBase64,
                                             const QByteArray &publicKey32)
{
    gLastError.clear();

    const QByteArray signature = QByteArray::fromBase64(signatureBase64.trimmed());
    if (signature.size() != 64) {
        gLastError = QStringLiteral("signature 长度无效。");
        return false;
    }

    const QByteArray publicKey = publicKey32.size() == 32 ? publicKey32 : activePublicKey();
    if (publicKey.size() != 32) {
        gLastError = QStringLiteral("公钥长度无效。");
        return false;
    }

    const int ok = ed25519_verify(reinterpret_cast<const unsigned char *>(signature.constData()),
                                  reinterpret_cast<const unsigned char *>(payload.constData()),
                                  static_cast<size_t>(payload.size()),
                                  reinterpret_cast<const unsigned char *>(publicKey.constData()));
    if (ok != 1) {
        gLastError = QStringLiteral("Ed25519 签名校验失败。");
        return false;
    }
    return true;
}

QString LicenseSignatureVerifier::lastError()
{
    return gLastError;
}

} // namespace NFSScanner::License
