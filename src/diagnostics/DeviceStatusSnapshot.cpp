#include "diagnostics/DeviceStatusSnapshot.h"

#include "app/AppVersion.h"
#include "core/DeviceManager.h"
#include "license/LicenseManager.h"

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace NFSScanner::Diagnostics {

namespace {

QJsonObject deviceBlock(const QString &type,
                        bool enabled,
                        bool connected,
                        const QString &status,
                        const QString &lastError,
                        const QJsonObject &extra = QJsonObject())
{
    QJsonObject object;
    object.insert(QStringLiteral("type"), type);
    object.insert(QStringLiteral("enabled"), enabled);
    object.insert(QStringLiteral("connected"), connected);
    object.insert(QStringLiteral("status"), status);
    object.insert(QStringLiteral("last_error"), lastError);
    for (auto it = extra.begin(); it != extra.end(); ++it) {
        object.insert(it.key(), it.value());
    }
    return object;
}

QString licenseMode(License::LicenseManager *licenseManager)
{
    if (!licenseManager) {
        return QStringLiteral("unknown");
    }
    switch (licenseManager->status()) {
    case License::LicenseStatus::Valid:
        return QStringLiteral("valid");
    case License::LicenseStatus::Demo:
        return QStringLiteral("demo");
    default:
        return QStringLiteral("invalid");
    }
}

} // namespace

bool DeviceStatusSnapshot::capture(Core::DeviceManager *deviceManager,
                                   License::LicenseManager *licenseManager,
                                   const QString &profileName,
                                   QJsonObject *object)
{
    if (!object || !deviceManager) {
        return false;
    }

    const Config::HardwareConfig hw = deviceManager->hardwareConfig();
    QJsonObject root;
    root.insert(QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("app_version"), QStringLiteral(APP_VERSION));
    root.insert(QStringLiteral("profile"), profileName);

    root.insert(QStringLiteral("motion"),
                deviceBlock(hw.motion.type,
                            hw.motion.enabled,
                            deviceManager->isMotionConnected(),
                            Core::deviceConnectionStateText(deviceManager->motionState()),
                            deviceManager->lastError()));

    QJsonObject spectrumExtra;
    if (deviceManager->isSpectrumConnected()) {
        spectrumExtra.insert(QStringLiteral("idn"), deviceManager->querySpectrumIdn());
    }
    root.insert(QStringLiteral("spectrum"),
                deviceBlock(hw.spectrum.type,
                            hw.spectrum.enabled,
                            deviceManager->isSpectrumConnected(),
                            Core::deviceConnectionStateText(deviceManager->spectrumState()),
                            deviceManager->lastError(),
                            spectrumExtra));

    root.insert(QStringLiteral("camera"),
                deviceBlock(hw.camera.type,
                            hw.camera.enabled,
                            deviceManager->isCameraConnected(),
                            Core::deviceConnectionStateText(deviceManager->cameraState()),
                            deviceManager->lastError()));

    QJsonObject probeExtra;
    probeExtra.insert(QStringLiteral("orientation"), hw.probe.orientation);
    root.insert(QStringLiteral("probe"),
                deviceBlock(hw.probe.type,
                            hw.probe.enabled,
                            deviceManager->isProbeConnected(),
                            Core::deviceConnectionStateText(deviceManager->probeState()),
                            deviceManager->lastError(),
                            probeExtra));

    QJsonObject license;
    license.insert(QStringLiteral("mode"), licenseMode(licenseManager));
    license.insert(QStringLiteral("valid"), licenseManager && (licenseManager->status() == License::LicenseStatus::Valid
                                                              || licenseManager->status() == License::LicenseStatus::Demo));
    root.insert(QStringLiteral("license"), license);

    *object = root;
    return true;
}

bool DeviceStatusSnapshot::saveToFile(const QString &path,
                                      Core::DeviceManager *deviceManager,
                                      License::LicenseManager *licenseManager,
                                      const QString &profileName)
{
    QJsonObject object;
    if (!capture(deviceManager, licenseManager, profileName, &object)) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return true;
}

QString DeviceStatusSnapshot::summaryText(const QJsonObject &object)
{
    return QStringLiteral("profile=%1 motion=%2 spectrum=%3 camera=%4 probe=%5")
        .arg(object.value(QStringLiteral("profile")).toString(),
             object.value(QStringLiteral("motion")).toObject().value(QStringLiteral("status")).toString(),
             object.value(QStringLiteral("spectrum")).toObject().value(QStringLiteral("status")).toString(),
             object.value(QStringLiteral("camera")).toObject().value(QStringLiteral("status")).toString(),
             object.value(QStringLiteral("probe")).toObject().value(QStringLiteral("orientation")).toString());
}

} // namespace NFSScanner::Diagnostics
