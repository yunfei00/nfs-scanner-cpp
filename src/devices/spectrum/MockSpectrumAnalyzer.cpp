#include "devices/spectrum/MockSpectrumAnalyzer.h"

#include <QDateTime>
#include <QRandomGenerator>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

namespace NFSScanner::Devices::Spectrum {

MockSpectrumAnalyzer::MockSpectrumAnalyzer(QObject *parent)
    : ISpectrumAnalyzer(parent)
{
}

QString MockSpectrumAnalyzer::name() const
{
    return QStringLiteral("Mock Spectrum");
}

bool MockSpectrumAnalyzer::connectDevice(const QVariantMap &options)
{
    Q_UNUSED(options)

    connected_ = true;
    lastError_.clear();
    emit connectedChanged(true);
    emit logMessage(QStringLiteral("Mock Spectrum 已连接。"));
    return true;
}

void MockSpectrumAnalyzer::disconnectDevice()
{
    if (!connected_) {
        return;
    }

    connected_ = false;
    emit connectedChanged(false);
    emit logMessage(QStringLiteral("Mock Spectrum 已断开。"));
}

bool MockSpectrumAnalyzer::isConnected() const
{
    return connected_;
}

bool MockSpectrumAnalyzer::configure(const SpectrumConfig &config)
{
    config_ = config;
    if (config_.sweepPoints < 2) {
        config_.sweepPoints = 2;
    }
    if (config_.stopFreqHz <= config_.startFreqHz) {
        config_.stopFreqHz = config_.startFreqHz + 1.0;
    }
    config_.centerFreqHz = (config_.startFreqHz + config_.stopFreqHz) * 0.5;
    config_.spanHz = config_.stopFreqHz - config_.startFreqHz;
    lastError_.clear();
    emit logMessage(QStringLiteral("Mock Spectrum 配置完成：%1 Hz ~ %2 Hz，%3 点。")
                        .arg(config_.startFreqHz, 0, 'f', 0)
                        .arg(config_.stopFreqHz, 0, 'f', 0)
                        .arg(config_.sweepPoints));
    return true;
}

QString MockSpectrumAnalyzer::queryIdn()
{
    return QStringLiteral("Mock Spectrum Analyzer v1.0");
}

SpectrumTrace MockSpectrumAnalyzer::singleSweep(int pointIndex, double x, double y, double z)
{
    Q_UNUSED(z)

    SpectrumTrace trace;
    trace.traceId = config_.traceId.trimmed().isEmpty() ? QStringLiteral("Trc1_S21") : config_.traceId;
    trace.timestamp = QDateTime::currentDateTime();
    trace.source = name();

    if (!connected_) {
        lastError_ = QStringLiteral("Mock Spectrum 未连接，无法执行单次扫描。");
        emit errorOccurred(lastError_);
        return trace;
    }

    const int pointCount = std::max(2, config_.sweepPoints);
    const double startHz = config_.startFreqHz;
    const double stopHz = std::max(config_.stopFreqHz, config_.startFreqHz + 1.0);
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
