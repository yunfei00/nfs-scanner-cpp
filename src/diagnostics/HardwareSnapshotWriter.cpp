#include "diagnostics/HardwareSnapshotWriter.h"

#include "config/HardwareConfig.h"
#include "core/DeviceManager.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace NFSScanner::Diagnostics {

bool HardwareSnapshotWriter::writeHardwareConfigSnapshot(const QString &directory,
                                                         const Config::HardwareConfig &config,
                                                         const QString &profileName)
{
    QDir dir(directory);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    QJsonObject root = Config::hardwareConfigToJson(config);
    if (!profileName.trimmed().isEmpty()) {
        root.insert(QStringLiteral("profile_name"), profileName);
    }

    const QString path = dir.filePath(QStringLiteral("hardware_config_snapshot.json"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool HardwareSnapshotWriter::writeDeviceStatusSnapshot(const QString &directory,
                                                       const Core::DeviceManager *deviceManager)
{
    if (!deviceManager) {
        return false;
    }

    QDir dir(directory);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    QJsonObject object;
    object.insert(QStringLiteral("motion"), Core::deviceConnectionStateText(deviceManager->motionState()));
    object.insert(QStringLiteral("motion_mock"), deviceManager->motionMockMode());
    object.insert(QStringLiteral("spectrum"), Core::deviceConnectionStateText(deviceManager->spectrumState()));
    object.insert(QStringLiteral("camera"), Core::deviceConnectionStateText(deviceManager->cameraState()));
    object.insert(QStringLiteral("probe"), Core::deviceConnectionStateText(deviceManager->probeState()));
    object.insert(QStringLiteral("last_error"), deviceManager->lastError());

    const QString path = dir.filePath(QStringLiteral("device_status_snapshot.json"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace NFSScanner::Diagnostics
