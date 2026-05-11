#include "devices/spectrum/MockSpectrumAnalyzer.h"

#include <QRandomGenerator>

namespace NFSScanner::Devices::Spectrum {

MockSpectrumAnalyzer::MockSpectrumAnalyzer(QObject *parent)
    : ISpectrumAnalyzer(parent)
{
}

QString MockSpectrumAnalyzer::name() const
{
    return QStringLiteral("Mock Spectrum Analyzer");
}

bool MockSpectrumAnalyzer::connectDevice()
{
    if (connected_) {
        return true;
    }

    connected_ = true;
    emit connectionChanged(true);
    emit centerFrequencyChanged(centerFrequencyMhz_);
    return true;
}

void MockSpectrumAnalyzer::disconnectDevice()
{
    if (!connected_) {
        return;
    }

    connected_ = false;
    emit connectionChanged(false);
}

bool MockSpectrumAnalyzer::isConnected() const
{
    return connected_;
}

void MockSpectrumAnalyzer::setCenterFrequency(double mhz)
{
    centerFrequencyMhz_ = mhz;
    emit centerFrequencyChanged(centerFrequencyMhz_);
}

double MockSpectrumAnalyzer::centerFrequencyMhz() const
{
    return centerFrequencyMhz_;
}

double MockSpectrumAnalyzer::readPowerDbm()
{
    if (!connected_) {
        emit errorOccurred(QStringLiteral("频谱仪未连接，无法读取功率。"));
        return -120.0;
    }

    return -72.0 + QRandomGenerator::global()->generateDouble() * 34.0;
}

} // namespace NFSScanner::Devices::Spectrum
