#pragma once

#include "devices/spectrum/ISpectrumAnalyzer.h"

namespace NFSScanner::Devices::Spectrum {

class MockSpectrumAnalyzer final : public ISpectrumAnalyzer
{
    Q_OBJECT

public:
    explicit MockSpectrumAnalyzer(QObject *parent = nullptr);

    QString name() const override;
    bool connectDevice() override;
    void disconnectDevice() override;
    bool isConnected() const override;
    void setCenterFrequency(double mhz) override;
    double centerFrequencyMhz() const override;
    double readPowerDbm() override;
    SpectrumTrace singleSweep(int pointIndex, double x, double y, double z) override;
    QString lastError() const override;

private:
    bool connected_ = true;
    double centerFrequencyMhz_ = 1000.0;
    QString lastError_;
};

} // namespace NFSScanner::Devices::Spectrum
