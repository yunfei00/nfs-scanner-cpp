#pragma once

#include <QObject>
#include <QString>

namespace NFSScanner::Devices::Probe {

enum class ProbeOrientation {
    Hx,
    Hy
};

QString probeOrientationName(ProbeOrientation orientation);
ProbeOrientation probeOrientationFromName(const QString &name);

class IProbeController : public QObject
{
    Q_OBJECT

public:
    explicit IProbeController(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~IProbeController() override = default;

    virtual QString name() const = 0;
    virtual bool connectDevice() = 0;
    virtual void disconnectDevice() = 0;
    virtual bool isConnected() const = 0;
    virtual bool setOrientation(ProbeOrientation orientation) = 0;
    virtual ProbeOrientation currentOrientation() const = 0;
    virtual QString lastError() const = 0;

signals:
    void logMessage(const QString &message);
    void connectedChanged(bool connected);
    void orientationChanged(ProbeOrientation orientation);
    void errorOccurred(const QString &message);
};

} // namespace NFSScanner::Devices::Probe
