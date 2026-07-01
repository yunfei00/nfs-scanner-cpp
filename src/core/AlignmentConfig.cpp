#include "core/AlignmentConfig.h"

#include <cmath>

namespace NFSScanner::Core {

void AlignmentConfig::syncCornersFromRectangle()
{
    pixelCorners = {
        QPointF(pixelXMin, pixelYMin),
        QPointF(pixelXMax, pixelYMin),
        QPointF(pixelXMax, pixelYMax),
        QPointF(pixelXMin, pixelYMax),
    };
    worldCorners = {
        QPointF(worldXMin, worldYMin),
        QPointF(worldXMax, worldYMin),
        QPointF(worldXMax, worldYMax),
        QPointF(worldXMin, worldYMax),
    };
}

bool AlignmentConfig::isValid() const
{
    if (!enabled) {
        return false;
    }

    if (mappingMode == AlignmentMappingMode::LinearRectangle) {
        return std::abs(worldXMax - worldXMin) > 1e-9
            && std::abs(worldYMax - worldYMin) > 1e-9
            && std::abs(pixelXMax - pixelXMin) > 1e-9
            && std::abs(pixelYMax - pixelYMin) > 1e-9;
    }

    if (pixelCorners.size() != 4 || worldCorners.size() != 4) {
        return false;
    }

    const auto area = [](const QVector<QPointF> &corners) {
        double sum = 0.0;
        for (int i = 0; i < 4; ++i) {
            const QPointF &a = corners.at(i);
            const QPointF &b = corners.at((i + 1) % 4);
            sum += a.x() * b.y() - b.x() * a.y();
        }
        return std::abs(sum) * 0.5;
    };

    return area(pixelCorners) > 1e-6 && area(worldCorners) > 1e-9;
}

} // namespace NFSScanner::Core
