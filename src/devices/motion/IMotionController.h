#pragma once

#include <QObject>
#include <QString>

namespace NFSScanner::Devices::Motion {

class IMotionController : public QObject
{
    Q_OBJECT

public:
    explicit IMotionController(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~IMotionController() override = default;

    virtual QString name() const = 0;
    virtual bool connectDevice() = 0;
    virtual void disconnectDevice() = 0;
    virtual bool isConnected() const = 0;
    virtual bool moveTo(double xMm, double yMm, double zMm) = 0;

signals:
    void connectionChanged(bool connected);
    void positionChanged(double xMm, double yMm, double zMm);
    void errorOccurred(const QString &message);
};

} // namespace NFSScanner::Devices::Motion
