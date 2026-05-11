#pragma once

#include <QImage>
#include <QSize>
#include <QWidget>

class QPainter;
class QPaintEvent;
class QRectF;

namespace NFSScanner::UI {

class HeatmapView final : public QWidget
{
    Q_OBJECT

public:
    explicit HeatmapView(QWidget *parent = nullptr);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
    double zoomFactor() const;

public slots:
    void setScanProgress(int currentPoint, int totalPoints);
    void setZoomFactor(double factor);
    void setHeatmapImage(const QImage &image);
    void clearHeatmap();
    void clearHeatmapImage();
    void setOpacityPercent(int percent);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void drawGrid(QPainter &painter, const QRectF &rect) const;
    QRectF cameraRect() const;
    QImage currentHeatmapImage(const QSize &targetSize);

    int currentPoint_ = 0;
    int totalPoints_ = 0;
    double zoomFactor_ = 1.0;
    int opacityPercent_ = 85;
    QImage externalHeatmapImage_;
    QImage cachedMockHeatmap_;
    QSize cachedMockSize_;
    double cachedProgress_ = -1.0;
};

} // namespace NFSScanner::UI
