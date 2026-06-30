#pragma once

#include <QImage>
#include <QPointF>
#include <QSize>
#include <QVector>
#include <QWidget>

class QMouseEvent;
class QPainter;
class QPaintEvent;
class QRectF;

namespace NFSScanner::UI {

class HeatmapView final : public QWidget
{
    Q_OBJECT

public:
    struct GridMapping
    {
        QVector<double> xs;
        QVector<double> ys;
        double z = 0.0;

        bool isValid() const { return xs.size() >= 2 && ys.size() >= 2; }
    };

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
    void setScanRegionOverlay(double xMin, double yMin, double xMax, double yMax,
                              const QVector<QPointF> &pathPoints);
    void clearScanRegionOverlay();
    void setGridMapping(const GridMapping &mapping);
    void clearGridMapping();
    void setCrosshairEnabled(bool enabled);
    void setBackgroundImage(const QImage &image);
    void clearBackgroundImage();

signals:
    void cursorSampleChanged(double worldX, double worldY, bool insideImage);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRectF externalHeatmapRect() const;
    void updateCursorFromPosition(const QPointF &pos);
    void drawCrosshair(QPainter &painter, const QRectF &imageRect) const;
    void drawGrid(QPainter &painter, const QRectF &rect) const;
    void drawScanOverlay(QPainter &painter, const QRectF &cameraRect) const;
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
    bool scanOverlayVisible_ = false;
    double regionXMin_ = 0.0;
    double regionYMin_ = 0.0;
    double regionXMax_ = 0.0;
    double regionYMax_ = 0.0;
    QVector<QPointF> pathPoints_;
    GridMapping gridMapping_;
    bool crosshairEnabled_ = false;
    bool cursorInsideImage_ = false;
    QPointF cursorPos_;
    QImage backgroundImage_;
    mutable QRectF cachedExternalHeatmapRect_;
};

} // namespace NFSScanner::UI
