#pragma once

#include <QString>

namespace NFSScanner::Core {

enum class HardwareMode {
    MockAll,
    RealMotionMockSpectrum,
    MockMotionRealSpectrum,
    RealMotionRealSpectrum
};

enum class ScanErrorStrategy {
    StopOnError,
    RetryThenStop,
    SkipPoint
};

QString hardwareModeToString(HardwareMode mode);
HardwareMode hardwareModeFromString(const QString &text);

QString scanErrorStrategyToString(ScanErrorStrategy strategy);
ScanErrorStrategy scanErrorStrategyFromString(const QString &text);

HardwareMode inferHardwareMode(bool motionMock, bool spectrumMock);

} // namespace NFSScanner::Core
