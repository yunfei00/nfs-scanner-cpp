#pragma once

#include "core/AlignmentConfig.h"

#include <QPointF>
#include <QString>

namespace NFSScanner::Core {

class AlignmentManager
{
public:
    void setConfig(const AlignmentConfig &config);
    AlignmentConfig config() const;

    bool loadFromFile(const QString &filePath);
    bool saveToFile(const QString &filePath) const;
    QString lastError() const;

    QPointF worldToPixel(double worldX, double worldY) const;
    QPointF pixelToWorld(double pixelX, double pixelY) const;

    // TODO(alignment): multi-point perspective calibration beyond linear rectangle mapping.

private:
    AlignmentConfig config_;
    mutable QString lastError_;
};

} // namespace NFSScanner::Core
