#include "core/ScanHardware.h"

namespace NFSScanner::Core {

QString hardwareModeToString(HardwareMode mode)
{
    switch (mode) {
    case HardwareMode::MockAll:
        return QStringLiteral("MockAll");
    case HardwareMode::RealMotionMockSpectrum:
        return QStringLiteral("RealMotionMockSpectrum");
    case HardwareMode::MockMotionRealSpectrum:
        return QStringLiteral("MockMotionRealSpectrum");
    case HardwareMode::RealMotionRealSpectrum:
        return QStringLiteral("RealMotionRealSpectrum");
    }
    return QStringLiteral("MockAll");
}

HardwareMode hardwareModeFromString(const QString &text)
{
    const QString normalized = text.trimmed();
    if (normalized.compare(QStringLiteral("RealMotionMockSpectrum"), Qt::CaseInsensitive) == 0) {
        return HardwareMode::RealMotionMockSpectrum;
    }
    if (normalized.compare(QStringLiteral("MockMotionRealSpectrum"), Qt::CaseInsensitive) == 0) {
        return HardwareMode::MockMotionRealSpectrum;
    }
    if (normalized.compare(QStringLiteral("RealMotionRealSpectrum"), Qt::CaseInsensitive) == 0) {
        return HardwareMode::RealMotionRealSpectrum;
    }
    return HardwareMode::MockAll;
}

QString scanErrorStrategyToString(ScanErrorStrategy strategy)
{
    switch (strategy) {
    case ScanErrorStrategy::StopOnError:
        return QStringLiteral("stopOnError");
    case ScanErrorStrategy::RetryThenStop:
        return QStringLiteral("retryThenStop");
    case ScanErrorStrategy::SkipPoint:
        return QStringLiteral("skipPoint");
    }
    return QStringLiteral("stopOnError");
}

ScanErrorStrategy scanErrorStrategyFromString(const QString &text)
{
    const QString normalized = text.trimmed();
    if (normalized.compare(QStringLiteral("retryThenStop"), Qt::CaseInsensitive) == 0) {
        return ScanErrorStrategy::RetryThenStop;
    }
    if (normalized.compare(QStringLiteral("skipPoint"), Qt::CaseInsensitive) == 0) {
        return ScanErrorStrategy::SkipPoint;
    }
    return ScanErrorStrategy::StopOnError;
}

HardwareMode inferHardwareMode(bool motionMock, bool spectrumMock)
{
    if (motionMock && spectrumMock) {
        return HardwareMode::MockAll;
    }
    if (!motionMock && spectrumMock) {
        return HardwareMode::RealMotionMockSpectrum;
    }
    if (motionMock && !spectrumMock) {
        return HardwareMode::MockMotionRealSpectrum;
    }
    return HardwareMode::RealMotionRealSpectrum;
}

} // namespace NFSScanner::Core
