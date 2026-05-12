#pragma once

#include <QMetaType>

namespace NFSScanner::Devices::Spectrum {

struct SpectrumAcquisitionRequest
{
    int pointIndex = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    int timeoutMs = 10000;
    int retryCount = 0;
};

} // namespace NFSScanner::Devices::Spectrum

Q_DECLARE_METATYPE(NFSScanner::Devices::Spectrum::SpectrumAcquisitionRequest)
