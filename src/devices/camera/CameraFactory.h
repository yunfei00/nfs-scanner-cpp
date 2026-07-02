#pragma once

#include "devices/camera/ICamera.h"

namespace NFSScanner::Devices::Camera {

class UsbCameraStub final : public ICamera
{
public:
    explicit UsbCameraStub(QObject *parent = nullptr);

    QString name() const override;
    bool connectDevice(const QVariantMap &options) override;
    void disconnectDevice() override;
    bool isConnected() const override;
    QImage captureFrame() override;
    QImage lastPreview() const override;
    QString lastError() const override;

private:
    QString lastError_;
};

class IndustrialCameraStub final : public ICamera
{
public:
    explicit IndustrialCameraStub(QObject *parent = nullptr);

    QString name() const override;
    bool connectDevice(const QVariantMap &options) override;
    void disconnectDevice() override;
    bool isConnected() const override;
    QImage captureFrame() override;
    QImage lastPreview() const override;
    QString lastError() const override;

private:
    QString lastError_;
};

ICamera *createCamera(const QString &type, QObject *parent = nullptr);
bool saveCameraImage(const QImage &image, const QString &directory, QString *savedPath = nullptr);

} // namespace NFSScanner::Devices::Camera
