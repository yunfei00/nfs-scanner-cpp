#pragma once

#include <QString>
#include <QStringList>

namespace NFSScanner::Devices::Spectrum {

struct DeviceBringupResult
{
    bool connected = false;
    bool idnOk = false;
    bool configureOk = false;
    bool sweepOk = false;
    bool traceOk = false;
    QString idn;
    int tracePointCount = 0;
    double freqStartHz = 0.0;
    double freqStopHz = 0.0;
    QStringList steps;
    QStringList errors;
    QStringList warnings;

    bool overallOk() const
    {
        return connected && idnOk && configureOk && sweepOk && traceOk;
    }

    QString markdownReport() const;
};

class SpectrumBringupRunner
{
public:
    static DeviceBringupResult runMockBringup(const QString &deviceType);
};

} // namespace NFSScanner::Devices::Spectrum
