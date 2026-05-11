#pragma once

#include <QMetaType>

namespace NFSScanner::Core {

struct ScanPoint
{
    int index = 0;
    int xIndex = 0;
    int yIndex = 0;
    double xMm = 0.0;
    double yMm = 0.0;
    double frequencyMhz = 0.0;
    double powerDbm = 0.0;
    double normalizedLevel = 0.0;
};

} // namespace NFSScanner::Core

Q_DECLARE_METATYPE(NFSScanner::Core::ScanPoint)
