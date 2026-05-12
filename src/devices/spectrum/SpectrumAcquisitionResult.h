#pragma once

#include "devices/spectrum/SpectrumTrace.h"

#include <QDateTime>
#include <QMetaType>
#include <QString>

namespace NFSScanner::Devices::Spectrum {

struct SpectrumAcquisitionResult
{
    bool ok = false;
    QString error;
    SpectrumTrace trace;
    int pointIndex = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    QDateTime timestamp;
};

} // namespace NFSScanner::Devices::Spectrum

Q_DECLARE_METATYPE(NFSScanner::Devices::Spectrum::SpectrumAcquisitionResult)
