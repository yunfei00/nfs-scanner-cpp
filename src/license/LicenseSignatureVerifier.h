#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace NFSScanner::License {

class LicenseSignatureVerifier
{
public:
    static QByteArray buildCanonicalPayload(const QJsonObject &licenseObject);
    static QByteArray embeddedPublicKey();
    static bool verifyEd25519(const QByteArray &payload,
                              const QByteArray &signatureBase64,
                              const QByteArray &publicKey32 = embeddedPublicKey());
    static QString lastError();

    static void setTestPublicKeyOverride(const QByteArray &publicKey32);
    static void clearTestPublicKeyOverride();

private:
    static QByteArray activePublicKey();
};

} // namespace NFSScanner::License
