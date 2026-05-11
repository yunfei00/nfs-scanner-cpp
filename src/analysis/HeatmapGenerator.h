#pragma once

#include <QColor>
#include <QImage>
#include <QSize>

namespace NFSScanner::Analysis {

class HeatmapGenerator final
{
public:
    HeatmapGenerator() = delete;

    static QImage createMockHeatmap(const QSize &size, double progress);
    static QColor colorForLevel(double level);
};

} // namespace NFSScanner::Analysis
