#include "analysis/HeatmapGenerator.h"

#include "analysis/LutManager.h"

#include <QColor>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace NFSScanner::Analysis {

namespace {

QRgb missingColor()
{
    return qRgba(47, 52, 58, 170);
}

} // namespace

HeatmapRenderResult HeatmapGenerator::generate(const FrequencyData &data, const HeatmapRenderOptions &options)
{
    lastError_.clear();
    HeatmapRenderResult result;

    if (!data.isValid()) {
        lastError_ = QStringLiteral("频谱数据无效，请先加载 traces.csv。");
        result.error = lastError_;
        return result;
    }

    const QVector<double> xs = data.xs();
    const QVector<double> ys = data.ys();
    const QVector<double> zs = data.zs();
    if (xs.isEmpty() || ys.isEmpty() || zs.isEmpty()) {
        lastError_ = QStringLiteral("坐标点不足，无法生成热力图。");
        result.error = lastError_;
        return result;
    }

    if (options.traceId.trimmed().isEmpty()) {
        lastError_ = QStringLiteral("Trace 为空，无法生成热力图。");
        result.error = lastError_;
        return result;
    }

    if (options.freqIndex < 0 || options.freqIndex >= data.frequencyCount()) {
        lastError_ = QStringLiteral("频率索引越界：%1").arg(options.freqIndex);
        result.error = lastError_;
        return result;
    }

    const double z = zs.first();
    QVector<double> values;
    QVector<bool> hasValues;
    values.reserve(xs.size() * ys.size());
    hasValues.reserve(xs.size() * ys.size());

    double vmin = std::numeric_limits<double>::max();
    double vmax = std::numeric_limits<double>::lowest();
    int validCount = 0;

    for (double y : ys) {
        for (double x : xs) {
            const bool hasValue = data.hasValue(x, y, z, options.traceId);
            hasValues.push_back(hasValue);
            if (!hasValue) {
                values.push_back(0.0);
                continue;
            }

            const double value = data.scalarValue(x, y, z, options.traceId, options.freqIndex, options.mode);
            values.push_back(value);
            vmin = std::min(vmin, value);
            vmax = std::max(vmax, value);
            ++validCount;
        }
    }

    if (validCount == 0) {
        lastError_ = QStringLiteral("当前 Trace/Frequency 没有可显示的数据。");
        result.error = lastError_;
        return result;
    }

    if (!options.autoRange) {
        vmin = options.vmin;
        vmax = options.vmax;
    }

    if (!std::isfinite(vmin) || !std::isfinite(vmax)) {
        vmin = 0.0;
        vmax = 1.0;
    }

    if (vmin >= vmax || qAbs(vmax - vmin) < 1e-12) {
        const double center = std::isfinite(vmin) ? vmin : 0.0;
        const double spread = std::max(1.0, std::abs(center) * 0.1);
        vmin = center - spread;
        vmax = center + spread;
    }

    const int cols = xs.size();
    const int rows = ys.size();
    QImage grid(cols, rows, QImage::Format_ARGB32);
    grid.fill(missingColor());

    for (int row = 0; row < rows; ++row) {
        auto *line = reinterpret_cast<QRgb *>(grid.scanLine(rows - 1 - row));
        for (int col = 0; col < cols; ++col) {
            const int index = row * cols + col;
            if (!hasValues.at(index)) {
                line[col] = missingColor();
                continue;
            }

            const double normalized = (values.at(index) - vmin) / (vmax - vmin);
            line[col] = LutManager::colorAt(options.lutName, normalized, options.alpha);
        }
    }

    const int safeWidth = std::max(1, options.width);
    const int safeHeight = std::max(1, options.height);
    result.image = grid.scaled(safeWidth, safeHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    result.colorbar = LutManager::createColorbar(options.lutName, 28, std::max(120, safeHeight / 2), options.alpha);
    result.actualVmin = vmin;
    result.actualVmax = vmax;
    result.ok = !result.image.isNull();
    if (!result.ok) {
        result.error = QStringLiteral("热力图图像生成失败。");
        lastError_ = result.error;
    }
    return result;
}

QImage HeatmapGenerator::generate(const FrequencyData &data,
                                  const QString &traceId,
                                  int freqIndex,
                                  const QString &mode,
                                  int width,
                                  int height)
{
    HeatmapRenderOptions options;
    options.traceId = traceId;
    options.freqIndex = freqIndex;
    options.mode = mode;
    options.width = width;
    options.height = height;
    const HeatmapRenderResult result = generate(data, options);
    return result.image;
}

QString HeatmapGenerator::lastError() const
{
    return lastError_;
}

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
