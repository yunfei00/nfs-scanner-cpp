#include "core/AlignmentConfig.h"

#include <cmath>

namespace NFSScanner::Core {

bool AlignmentConfig::isValid() const
{
    return enabled
        && std::abs(worldXMax - worldXMin) > 1e-9
        && std::abs(worldYMax - worldYMin) > 1e-9
        && std::abs(pixelXMax - pixelXMin) > 1e-9
        && std::abs(pixelYMax - pixelYMin) > 1e-9;
}

} // namespace NFSScanner::Core
