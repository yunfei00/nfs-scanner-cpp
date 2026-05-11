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

private:
    bool connected_ = false;
    double centerFrequencyMhz_ = 1000.0;
};

} // namespace NFSScanner::Devices::Spectrum
