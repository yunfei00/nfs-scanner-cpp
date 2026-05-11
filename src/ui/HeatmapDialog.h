#pragma once

#include <QDialog>
#include <QImage>
#include <QString>

class QLabel;
class QPushButton;
class QResizeEvent;

namespace NFSScanner::UI {

class HeatmapDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit HeatmapDialog(const QImage &image, const QString &title, QWidget *parent = nullptr);
    HeatmapDialog(const QImage &image,
                  const QImage &colorbar,
                  const QString &title,
                  double vmin,
                  double vmax,
                  QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void exportPng();
    void updatePixmap();
    QImage exportImage() const;

    QImage image_;
    QImage colorbar_;
    double vmin_ = 0.0;
    double vmax_ = 0.0;
    QLabel *titleLabel_ = nullptr;
    QLabel *imageLabel_ = nullptr;
    QLabel *colorbarLabel_ = nullptr;
    QLabel *colorbarMinLabel_ = nullptr;
    QLabel *colorbarMaxLabel_ = nullptr;
    QPushButton *exportButton_ = nullptr;
};

} // namespace NFSScanner::UI
