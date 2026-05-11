#include "devices/spectrum/MockSpectrumAnalyzer.h"

#include <QDateTime>
#include <QRandomGenerator>

#include <algorithm>
#include <cmath>

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

SpectrumTrace MockSpectrumAnalyzer::singleSweep(int pointIndex, double x, double y, double z)
{
    Q_UNUSED(z)

    SpectrumTrace trace;
    trace.timestamp = QDateTime::currentDateTime();

    if (!connected_) {
        lastError_ = QStringLiteral("频谱仪未连接，无法执行单次扫描。");
        emit errorOccurred(lastError_);
        return trace;
    }

    constexpr int pointCount = 201;
    constexpr double startHz = 1'000'000.0;
    constexpr double stopHz = 1'000'000'000.0;
    constexpr double pi2 = 6.283185307179586;
    constexpr double centerX = 5.0;
    constexpr double centerY = 5.0;
    constexpr double sigma = 32.0;

    trace.freqs.reserve(pointCount);
    trace.values.reserve(pointCount);

    const double spatialDistance = (x - centerX) * (x - centerX) + (y - centerY) * (y - centerY);
    const double spatial = std::exp(-spatialDistance / sigma);
    const double phase = static_cast<double>(std::max(0, pointIndex)) * 0.031;

    for (int i = 0; i < pointCount; ++i) {
        const double normalized = pointCount > 1 ? static_cast<double>(i) / static_cast<double>(pointCount - 1) : 0.0;
        const double freq = startHz + normalized * (stopHz - startHz);
        const double ripple = std::sin(normalized * pi2 * 3.0 + phase);
        const double noise = QRandomGenerator::global()->generateDouble() * 1.0 - 0.5;
        const double value = -60.0 + 20.0 * spatial + 5.0 * ripple + noise;

        trace.freqs.push_back(freq);
        trace.values.push_back(value);
    }

    lastError_.clear();
    return trace;
}

QString MockSpectrumAnalyzer::lastError() const
{
    return lastError_;
}

} // namespace NFSScanner::Devices::Spectrum
