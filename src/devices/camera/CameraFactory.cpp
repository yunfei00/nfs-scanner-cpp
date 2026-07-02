#include "devices/camera/CameraFactory.h"

#include "devices/camera/MockCamera.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

namespace NFSScanner::Devices::Camera {

UsbCameraStub::UsbCameraStub(QObject *parent)
    : ICamera(parent)
{
}

QString UsbCameraStub::name() const
{
    return QStringLiteral("USB Camera (Stub)");
}

bool UsbCameraStub::connectDevice(const QVariantMap &options)
{
    Q_UNUSED(options)
    lastError_ = QStringLiteral("USB 相机需要后续接入 OpenCV / 厂商 SDK，当前不可用。");
    emit logMessage(lastError_);
    return false;
}

void UsbCameraStub::disconnectDevice()
{
}

bool UsbCameraStub::isConnected() const
{
    return false;
}

QImage UsbCameraStub::captureFrame()
{
    lastError_ = QStringLiteral("USB 相机未接入。");
    return {};
}

QImage UsbCameraStub::lastPreview() const
{
    return {};
}

QString UsbCameraStub::lastError() const
{
    return lastError_;
}

IndustrialCameraStub::IndustrialCameraStub(QObject *parent)
    : ICamera(parent)
{
}

QString IndustrialCameraStub::name() const
{
    return QStringLiteral("Industrial Camera (Stub)");
}

bool IndustrialCameraStub::connectDevice(const QVariantMap &options)
{
    Q_UNUSED(options)
    lastError_ = QStringLiteral("工业相机待厂商 SDK 接入，当前不可用。");
    emit logMessage(lastError_);
    return false;
}

void IndustrialCameraStub::disconnectDevice()
{
}

bool IndustrialCameraStub::isConnected() const
{
    return false;
}

QImage IndustrialCameraStub::captureFrame()
{
    lastError_ = QStringLiteral("工业相机未接入。");
    return {};
}

QImage IndustrialCameraStub::lastPreview() const
{
    return {};
}

QString IndustrialCameraStub::lastError() const
{
    return lastError_;
}

ICamera *createCamera(const QString &type, QObject *parent)
{
    const QString normalized = type.trimmed().toLower();
    if (normalized.isEmpty() || normalized.contains(QStringLiteral("mock"))) {
        return new MockCamera(parent);
    }
    if (normalized.contains(QStringLiteral("usb"))) {
        return new UsbCameraStub(parent);
    }
    if (normalized.contains(QStringLiteral("industrial")) || normalized.contains(QStringLiteral("sdk"))) {
        return new IndustrialCameraStub(parent);
    }
    return new MockCamera(parent);
}

bool saveCameraImage(const QImage &image, const QString &directory, QString *savedPath)
{
    if (image.isNull()) {
        return false;
    }

    QDir dir(directory);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    const QString fileName = QStringLiteral("capture_%1.png")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = dir.filePath(fileName);
    if (!image.save(path)) {
        return false;
    }

    if (savedPath) {
        *savedPath = path;
    }
    return true;
}

} // namespace NFSScanner::Devices::Camera
