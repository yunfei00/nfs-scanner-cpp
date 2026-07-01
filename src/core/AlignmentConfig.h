#pragma once

#include <QPointF>
#include <QString>
#include <QVector>

namespace NFSScanner::Core {

enum class AlignmentMappingMode {
    LinearRectangle,
    PerspectiveFourPoint,
};

struct AlignmentConfig
{
    QString backgroundImagePath;
    AlignmentMappingMode mappingMode = AlignmentMappingMode::LinearRectangle;
    double worldXMin = 0.0;
    double worldXMax = 200.0;
    double worldYMin = -300.0;
    double worldYMax = 0.0;
    double worldZ = 0.0;
    double pixelXMin = 0.0;
    double pixelXMax = 640.0;
    double pixelYMin = 0.0;
    double pixelYMax = 480.0;
    QVector<QPointF> pixelCorners;
    QVector<QPointF> worldCorners;
    bool fixedAspectRatio = true;
    bool enabled = false;

    bool isValid() const;
    void syncCornersFromRectangle();
};

} // namespace NFSScanner::Core
