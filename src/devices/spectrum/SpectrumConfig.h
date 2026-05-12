#pragma once

#include <QString>

namespace NFSScanner::Devices::Spectrum {

struct SpectrumConfig
{
    double startFreqHz = 1e6;
    double stopFreqHz = 1e9;
    double centerFreqHz = 500e6;
    double spanHz = 999e6;
    double rbwHz = 100e3;
    double vbwHz = 100e3;
    int sweepPoints = 201;
    double sweepTimeSec = 0.1;
    QString traceId = QStringLiteral("Trc1_S21");
};

} // namespace NFSScanner::Devices::Spectrum
