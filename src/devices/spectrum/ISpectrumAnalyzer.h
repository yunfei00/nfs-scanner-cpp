#pragma once

#include "devices/spectrum/SpectrumConfig.h"
#include "devices/spectrum/SpectrumTrace.h"

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace NFSScanner::Devices::Spectrum {

class ISpectrumAnalyzer : public QObject
{
    Q_OBJECT

public:
    explicit ISpectrumAnalyzer(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~ISpectrumAnalyzer() override = default;

    virtual QString name() const = 0;
    virtual bool connectDevice(const QVariantMap &options) = 0;
    virtual void disconnectDevice() = 0;
    virtual bool isConnected() const = 0;
    virtual bool configure(const SpectrumConfig &config) = 0;
    virtual SpectrumTrace singleSweep(int pointIndex, double x, double y, double z) = 0;
    virtual QString queryIdn() = 0;
    virtual QString lastError() const = 0;

signals:
    void logMessage(const QString &message);
    void errorOccurred(const QString &message);
    void connectedChanged(bool connected);
};

} // namespace NFSScanner::Devices::Spectrum
