#pragma once

#include "analysis/FrequencyData.h"

#include <QColor>
#include <QImage>
#include <QSize>
#include <QString>

namespace NFSScanner::Analysis {

struct HeatmapRenderOptions
{
    QString traceId;
    int freqIndex = 0;
    QString mode = QStringLiteral("magnitude");
    QString lutName = QStringLiteral("turbo");
    bool autoRange = true;
    double vmin = 0.0;
    double vmax = 1.0;
    int alpha = 220;
    int width = 800;
    int height = 600;
};

struct HeatmapRenderResult
{
    QImage image;
    QImage colorbar;
    double actualVmin = 0.0;
    double actualVmax = 0.0;
    bool ok = false;
    QString error;
};

class HeatmapGenerator
{
public:
    HeatmapGenerator() = default;

    HeatmapRenderResult generate(const FrequencyData &data, const HeatmapRenderOptions &options);
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
