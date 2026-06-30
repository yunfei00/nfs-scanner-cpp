#pragma once

#include "devices/camera/ICamera.h"

namespace NFSScanner::Devices::Camera {

class MockCamera final : public ICamera
{
    Q_OBJECT

public:
    explicit MockCamera(QObject *parent = nullptr);

    QString name() const override;
    bool connectDevice(const QVariantMap &options) override;
    void disconnectDevice() override;
    bool isConnected() const override;
    QImage captureFrame() override;
    QImage lastPreview() const override;
    QString lastError() const override;

private:
    QImage generateMockFrame() const;

    bool connected_ = false;
    QImage lastPreview_;
    QString lastError_;
};

} // namespace NFSScanner::Devices::Camera
