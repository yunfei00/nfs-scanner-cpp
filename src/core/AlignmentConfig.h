#pragma once

#include <QString>

namespace NFSScanner::Core {

struct AlignmentConfig
{
    QString backgroundImagePath;
    double worldXMin = 0.0;
    double worldXMax = 200.0;
    double worldYMin = -300.0;
    double worldYMax = 0.0;
    double pixelXMin = 0.0;
    double pixelXMax = 640.0;
    double pixelYMin = 0.0;
    double pixelYMax = 480.0;
    bool fixedAspectRatio = true;
    bool enabled = false;

    bool isValid() const;
};

} // namespace NFSScanner::Core
