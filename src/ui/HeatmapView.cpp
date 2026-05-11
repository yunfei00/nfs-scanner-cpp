#include "ui/HeatmapView.h"

#include <QColor>
#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QtMath>
#include <QtGlobal>

#include "analysis/HeatmapGenerator.h"

#include <algorithm>

namespace NFSScanner::UI {

HeatmapView::HeatmapView(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("heatmapView"));
    setAutoFillBackground(false);
    setMinimumSize(320, 220);
}

QSize HeatmapView::minimumSizeHint() const
{
    return {320, 220};
}

QSize HeatmapView::sizeHint() const
{
    return {520, 300};
}

double HeatmapView::zoomFactor() const
{
    return zoomFactor_;
}

void HeatmapView::setScanProgress(int currentPoint, int totalPoints)
{
    currentPoint_ = std::max(0, currentPoint);
    totalPoints_ = std::max(0, totalPoints);
    update();
}

void HeatmapView::setZoomFactor(double factor)
{
    zoomFactor_ = std::clamp(factor, 0.25, 4.0);
    update();
}

void HeatmapView::setHeatmapImage(const QImage &image)
{
    externalHeatmapImage_ = image;
    update();
}

void HeatmapView::clearHeatmap()
{
    clearHeatmapImage();
}

void HeatmapView::clearHeatmapImage()
{
    externalHeatmapImage_ = {};
    cachedMockHeatmap_ = {};
    cachedMockSize_ = {};
    cachedProgress_ = -1.0;
    update();
}

void HeatmapView::setOpacityPercent(int percent)
{
    opacityPercent_ = std::clamp(percent, 0, 100);
    update();
}

void HeatmapView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(13, 16, 21));

    drawGrid(painter, QRectF(rect()));

    if (!externalHeatmapImage_.isNull()) {
        const QRectF bounds = QRectF(rect()).adjusted(28.0, 48.0, -28.0, -28.0);
        const QSizeF imageSize = externalHeatmapImage_.size();
        const double scale = std::min(bounds.width() / std::max(1.0, imageSize.width()),
                                      bounds.height() / std::max(1.0, imageSize.height()));
        const QSizeF targetSize(imageSize.width() * scale, imageSize.height() * scale);
        const QRectF target(bounds.center().x() - targetSize.width() / 2.0,
                            bounds.center().y() - targetSize.height() / 2.0,
                            targetSize.width(),
                            targetSize.height());

        painter.save();
        painter.setOpacity(static_cast<double>(opacityPercent_) / 100.0);
        painter.drawImage(target, externalHeatmapImage_);
        painter.restore();

        painter.setPen(QPen(QColor(95, 116, 139), 1.4));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(target.adjusted(-1.0, -1.0, 1.0, 1.0), 6.0, 6.0);

        painter.setPen(QColor(220, 228, 238));
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 10, QFont::DemiBold));
        painter.drawText(rect().adjusted(18, 14, -18, -14),
                         Qt::AlignLeft | Qt::AlignTop,
                         QStringLiteral("Heatmap Preview"));

        painter.setFont(QFont(QStringLiteral("Segoe UI"), 10));
        painter.setPen(QColor(187, 199, 214));
        painter.drawText(rect().adjusted(18, 14, -18, -14),
                         Qt::AlignRight | Qt::AlignTop,
                         QStringLiteral("size: %1x%2 | opacity: %3%")
                             .arg(externalHeatmapImage_.width())
                             .arg(externalHeatmapImage_.height())
                             .arg(opacityPercent_));
        return;
    }

    const QRectF camera = cameraRect();
    QPainterPath cameraPath;
    cameraPath.addRoundedRect(camera, 8.0, 8.0);

    painter.save();
    painter.setClipPath(cameraPath);

    QLinearGradient cameraGradient(camera.topLeft(), camera.bottomRight());
    cameraGradient.setColorAt(0.0, QColor(30, 36, 44));
    cameraGradient.setColorAt(0.45, QColor(19, 23, 30));
    cameraGradient.setColorAt(1.0, QColor(35, 39, 43));
    painter.fillRect(camera, cameraGradient);

    painter.setPen(QPen(QColor(255, 255, 255, 28), 1));
    const double cell = 28.0 * zoomFactor_;
    for (double x = camera.left(); x < camera.right(); x += cell) {
        painter.drawLine(QPointF(x, camera.top()), QPointF(x, camera.bottom()));
    }
    for (double y = camera.top(); y < camera.bottom(); y += cell) {
        painter.drawLine(QPointF(camera.left(), y), QPointF(camera.right(), y));
    }

    const QRectF heatmapRect = camera.adjusted(24.0, 24.0, -24.0, -24.0);
    const QImage heatmap = currentHeatmapImage(heatmapRect.size().toSize());
    if (!heatmap.isNull()) {
        painter.setOpacity(0.88);
        painter.drawImage(heatmapRect, heatmap);
        painter.setOpacity(1.0);
    }

    const double progress = totalPoints_ > 0
        ? std::clamp(static_cast<double>(currentPoint_) / static_cast<double>(totalPoints_), 0.0, 1.0)
        : 0.0;
    const QPointF marker(
        heatmapRect.left() + heatmapRect.width() * progress,
        heatmapRect.center().y() + qSin(progress * 6.28318530718 * 2.0) * heatmapRect.height() * 0.26);

    painter.setBrush(QColor(255, 255, 255, 230));
    painter.setPen(QPen(QColor(12, 15, 18), 2));
    painter.drawEllipse(marker, 6.0, 6.0);

    painter.restore();

    painter.setPen(QPen(QColor(95, 116, 139), 1.4));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(camera, 8.0, 8.0);

    painter.setPen(QColor(220, 228, 238));
    painter.setFont(QFont(QStringLiteral("Segoe UI"), 10, QFont::DemiBold));
    painter.drawText(camera.adjusted(16.0, 14.0, -16.0, -14.0),
                     Qt::AlignLeft | Qt::AlignTop,
                     QStringLiteral("Mock Camera Frame"));

    const QString zoomText = QStringLiteral("Zoom: %1%").arg(qRound(zoomFactor_ * 100.0));
    const QString progressText = totalPoints_ > 0
        ? QStringLiteral("Scan: %1 / %2").arg(currentPoint_).arg(totalPoints_)
        : QStringLiteral("Scan: idle");

    painter.setFont(QFont(QStringLiteral("Segoe UI"), 10));
    painter.setPen(QColor(187, 199, 214));
    painter.drawText(rect().adjusted(20, 18, -20, -18),
                     Qt::AlignLeft | Qt::AlignTop,
                     zoomText);
    painter.drawText(rect().adjusted(20, 18, -20, -18),
                     Qt::AlignRight | Qt::AlignTop,
                     progressText);
}

void HeatmapView::drawGrid(QPainter &painter, const QRectF &rect) const
{
    painter.save();
    painter.setPen(QPen(QColor(255, 255, 255, 14), 1));

    constexpr double spacing = 32.0;
    for (double x = rect.left(); x <= rect.right(); x += spacing) {
        painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }

    for (double y = rect.top(); y <= rect.bottom(); y += spacing) {
        painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }

    painter.restore();
}

QRectF HeatmapView::cameraRect() const
{
    const QRectF bounds = QRectF(rect()).adjusted(36.0, 52.0, -36.0, -38.0);
    const double aspect = 4.0 / 3.0;
    double width = bounds.width();
    double height = width / aspect;

    if (height > bounds.height()) {
        height = bounds.height();
        width = height * aspect;
    }

    const QPointF topLeft(bounds.center().x() - width / 2.0,
                          bounds.center().y() - height / 2.0);
    return QRectF(topLeft, QSizeF(width, height));
}

QImage HeatmapView::currentHeatmapImage(const QSize &targetSize)
{
    if (!externalHeatmapImage_.isNull()) {
        return externalHeatmapImage_;
    }

    const double progress = totalPoints_ > 0
        ? std::clamp(static_cast<double>(currentPoint_) / static_cast<double>(totalPoints_), 0.0, 1.0)
        : 0.0;

    if (cachedMockHeatmap_.isNull()
        || cachedMockSize_ != targetSize
        || qAbs(cachedProgress_ - progress) > 0.002) {
        cachedMockSize_ = targetSize;
        cachedProgress_ = progress;
        cachedMockHeatmap_ = Analysis::HeatmapGenerator::createMockHeatmap(targetSize, progress);
    }

    return cachedMockHeatmap_;
}

} // namespace NFSScanner::UI
