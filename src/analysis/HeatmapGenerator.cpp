#include "analysis/HeatmapGenerator.h"

#include <QtMath>

#include <algorithm>

namespace NFSScanner::Analysis {

QImage HeatmapGenerator::createMockHeatmap(const QSize &size, double progress)
{
    if (!size.isValid() || size.isEmpty()) {
        return {};
    }

    const double clampedProgress = std::clamp(progress, 0.0, 1.0);
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    for (int y = 0; y < size.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        const double ny = static_cast<double>(y) / static_cast<double>(std::max(1, size.height() - 1));

        for (int x = 0; x < size.width(); ++x) {
            const double nx = static_cast<double>(x) / static_cast<double>(std::max(1, size.width() - 1));
            const double distance = qSqrt(qPow(nx - 0.58, 2.0) + qPow(ny - 0.43, 2.0));
            constexpr double pi = 3.14159265358979323846;
            const double ripple = 0.5 + 0.5 * qSin((nx * 9.0 + ny * 5.0 + clampedProgress * 2.5) * pi);
            const double level = std::clamp(1.0 - distance * 1.75 + ripple * 0.22, 0.0, 1.0);
            const double reveal = nx <= clampedProgress + 0.02 ? 1.0 : 0.22;

            QColor color = colorForLevel(level);
            color.setAlphaF(std::clamp((0.22 + level * 0.72) * reveal, 0.0, 0.86));
            line[x] = color.rgba();
        }
    }

    return image;
}

QColor HeatmapGenerator::colorForLevel(double level)
{
    const double value = std::clamp(level, 0.0, 1.0);

    if (value < 0.25) {
        const double t = value / 0.25;
        return QColor::fromRgbF(0.03, 0.16 + t * 0.23, 0.38 + t * 0.36);
    }

    if (value < 0.55) {
        const double t = (value - 0.25) / 0.30;
        return QColor::fromRgbF(0.02, 0.39 + t * 0.40, 0.74 - t * 0.28);
    }

    if (value < 0.78) {
        const double t = (value - 0.55) / 0.23;
        return QColor::fromRgbF(0.08 + t * 0.88, 0.79 + t * 0.10, 0.38 - t * 0.25);
    }

    const double t = (value - 0.78) / 0.22;
    return QColor::fromRgbF(0.96, 0.89 - t * 0.28, 0.13 - t * 0.08);
}

} // namespace NFSScanner::Analysis
