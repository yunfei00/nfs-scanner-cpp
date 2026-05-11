#pragma once

#include "analysis/FrequencyData.h"

#include <QColor>
#include <QImage>
#include <QSize>
#include <QString>

namespace NFSScanner::Analysis {

class HeatmapGenerator
{
public:
    HeatmapGenerator() = default;

    QImage generate(const FrequencyData &data,
                    const QString &traceId,
                    int freqIndex,
                    const QString &mode,
                    int width = 600,
                    int height = 400);
    QString lastError() const;

    static QImage createMockHeatmap(const QSize &size, double progress);
    static QColor colorForLevel(double level);

private:
    QString lastError_;
};

} // namespace NFSScanner::Analysis
