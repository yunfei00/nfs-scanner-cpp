#include "core/ScanPathPlanner.h"

#include <algorithm>
#include <cmath>

namespace NFSScanner::Core {

namespace {

QVector<double> generateAxisValues(double start, double end, double step)
{
    QVector<double> values;
    constexpr double epsilon = 1e-9;
    constexpr int maxAxisPoints = 1000000;
    const double direction = start <= end ? 1.0 : -1.0;
    const double signedStep = std::abs(step) * direction;

    double value = start;
    int guard = 0;
    if (direction > 0.0) {
        while (value <= end + epsilon && guard < maxAxisPoints) {
            values.push_back(value);
            value += signedStep;
            ++guard;
        }
    } else {
        while (value >= end - epsilon && guard < maxAxisPoints) {
            values.push_back(value);
            value += signedStep;
            ++guard;
        }
    }

    if (values.isEmpty() || std::abs(values.last() - end) > epsilon) {
        values.push_back(end);
    }

    return values;
}

} // namespace

QVector<ScanPoint> ScanPathPlanner::generate(const ScanConfig &config)
{
    lastError_.clear();

    if (config.stepX <= 0.0) {
        lastError_ = QStringLiteral("stepX 必须大于 0。");
        return {};
    }

    if (config.stepY <= 0.0) {
        lastError_ = QStringLiteral("stepY 必须大于 0。");
        return {};
    }

    const QVector<double> xs = generateAxisValues(config.startX, config.endX, config.stepX);
    const QVector<double> ys = generateAxisValues(config.startY, config.endY, config.stepY);

    QVector<ScanPoint> points;
    points.reserve(xs.size() * ys.size());

    int index = 1;
    for (int yIndex = 0; yIndex < ys.size(); ++yIndex) {
        QVector<double> rowXs = xs;
        if (config.snakeMode && yIndex % 2 == 1) {
            std::reverse(rowXs.begin(), rowXs.end());
        }

        for (int xIndex = 0; xIndex < rowXs.size(); ++xIndex) {
            ScanPoint point;
            point.index = index++;
            point.x = rowXs.at(xIndex);
            point.y = ys.at(yIndex);
            point.z = config.startZ;
            point.xIndex = xIndex;
            point.yIndex = yIndex;
            point.xMm = point.x;
            point.yMm = point.y;
            points.push_back(point);
        }
    }

    if (points.isEmpty()) {
        lastError_ = QStringLiteral("未生成任何扫描点，请检查扫描区域配置。");
    }

    return points;
}

QString ScanPathPlanner::lastError() const
{
    return lastError_;
}

} // namespace NFSScanner::Core
