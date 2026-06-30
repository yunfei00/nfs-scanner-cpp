#pragma once

#include "devices/spectrum/SpectrumConfig.h"
#include "devices/spectrum/SpectrumTrace.h"

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace NFSScanner::Devices::Spectrum {

class ISpectrumAnalyzer;

class SpectrumDeviceHost final : public QObject
{
    Q_OBJECT

public:
    explicit SpectrumDeviceHost(QObject *parent = nullptr);
    ~SpectrumDeviceHost() override;

    ISpectrumAnalyzer *analyzer() const;

public slots:
    bool createAnalyzer(const QString &analyzerName);
    void releaseAnalyzer();
    bool connectDevice(const QVariantMap &options);
    void disconnectDevice();
    bool configureDevice(const SpectrumConfig &config);
    QString queryIdn();
    SpectrumTrace singleSweep(int pointIndex, double x, double y, double z);
    QString lastError() const;

signals:
    void logMessage(const QString &message);
    void connectedChanged(bool connected);

private:
    ISpectrumAnalyzer *analyzer_ = nullptr;
    QString lastError_;
};

} // namespace NFSScanner::Devices::Spectrum
