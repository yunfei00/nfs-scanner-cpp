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

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void exportPng();
    void updatePixmap();

    QImage image_;
    QLabel *titleLabel_ = nullptr;
    QLabel *imageLabel_ = nullptr;
    QPushButton *exportButton_ = nullptr;
};

} // namespace NFSScanner::UI
