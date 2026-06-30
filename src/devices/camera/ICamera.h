#pragma once

#include <QImage>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace NFSScanner::Devices::Camera {

class ICamera : public QObject
{
    Q_OBJECT

public:
    explicit ICamera(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~ICamera() override = default;

    virtual QString name() const = 0;
    virtual bool connectDevice(const QVariantMap &options) = 0;
    virtual void disconnectDevice() = 0;
    virtual bool isConnected() const = 0;
    virtual QImage captureFrame() = 0;
    virtual QImage lastPreview() const = 0;
    virtual QString lastError() const = 0;

signals:
    void logMessage(const QString &message);
    void connectedChanged(bool connected);
    void frameCaptured(const QImage &frame);
};

} // namespace NFSScanner::Devices::Camera
