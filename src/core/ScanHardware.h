#pragma once

#include <QString>

namespace NFSScanner::Core {

enum class HardwareMode {
    MockAll,
    RealMotionMockSpectrum,
    MockMotionRealSpectrum,
    RealMotionRealSpectrum
};

enum class ScanErrorPolicy {
    StopOnError,
    RetryThenStop,
    SkipPoint,
    ManualConfirm,
    MockFallbackExplicit
};

using ScanErrorStrategy = ScanErrorPolicy;

QString hardwareModeToString(HardwareMode mode);
HardwareMode hardwareModeFromString(const QString &text);

QString scanErrorPolicyToString(ScanErrorPolicy policy);
ScanErrorPolicy scanErrorPolicyFromString(const QString &text);

inline QString scanErrorStrategyToString(ScanErrorPolicy policy)
{
    return scanErrorPolicyToString(policy);
}

inline ScanErrorPolicy scanErrorStrategyFromString(const QString &text)
{
    return scanErrorPolicyFromString(text);
}

HardwareMode inferHardwareMode(bool motionMock, bool spectrumMock);

} // namespace NFSScanner::Core
