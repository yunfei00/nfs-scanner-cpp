#include "ui/HeatmapDialog.h"

#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace NFSScanner::UI {

HeatmapDialog::HeatmapDialog(const QImage &image, const QString &title, QWidget *parent)
    : HeatmapDialog(image, QImage(), title, 0.0, 0.0, parent)
{
}

HeatmapDialog::HeatmapDialog(const QImage &image,
                             const QImage &colorbar,
                             const QString &title,
                             double vmin,
                             double vmax,
                             QWidget *parent)
    : QDialog(parent)
    , image_(image)
    , colorbar_(colorbar)
    , vmin_(vmin)
    , vmax_(vmax)
{
    setWindowTitle(title);
    resize(900, 650);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    titleLabel_ = new QLabel(title, this);
    titleLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    titleLabel_->setWordWrap(true);
    layout->addWidget(titleLabel_);

    auto *content = new QWidget(this);
    auto *contentLayout = new QHBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(10);

    imageLabel_ = new QLabel(content);
    imageLabel_->setAlignment(Qt::AlignCenter);
    imageLabel_->setMinimumSize(480, 320);
    imageLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    imageLabel_->setStyleSheet(QStringLiteral("background:#20252b;border:1px solid #75808a;"));
    contentLayout->addWidget(imageLabel_, 1);

    auto *colorbarPanel = new QWidget(content);
    auto *colorbarLayout = new QVBoxLayout(colorbarPanel);
    colorbarLayout->setContentsMargins(0, 0, 0, 0);
    colorbarLayout->setSpacing(4);
    colorbarMaxLabel_ = new QLabel(QString::number(vmax_, 'g', 6), colorbarPanel);
    colorbarMaxLabel_->setAlignment(Qt::AlignCenter);
    colorbarLabel_ = new QLabel(colorbarPanel);
    colorbarLabel_->setAlignment(Qt::AlignCenter);
    colorbarLabel_->setFixedWidth(52);
    colorbarLabel_->setMinimumHeight(300);
    colorbarLabel_->setStyleSheet(QStringLiteral("background:#20252b;border:1px solid #75808a;"));
    colorbarMinLabel_ = new QLabel(QString::number(vmin_, 'g', 6), colorbarPanel);
    colorbarMinLabel_->setAlignment(Qt::AlignCenter);
    colorbarLayout->addWidget(colorbarMaxLabel_);
    colorbarLayout->addWidget(colorbarLabel_, 1);
    colorbarLayout->addWidget(colorbarMinLabel_);
    contentLayout->addWidget(colorbarPanel);
    layout->addWidget(content, 1);

    auto *buttonRow = new QWidget(this);
    auto *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addStretch(1);

    exportButton_ = new QPushButton(QStringLiteral("导出 PNG"), buttonRow);
    auto *closeButton = new QPushButton(QStringLiteral("关闭"), buttonRow);
    buttonLayout->addWidget(exportButton_);
    buttonLayout->addWidget(closeButton);
    layout->addWidget(buttonRow);

    connect(exportButton_, &QPushButton::clicked, this, &HeatmapDialog::exportPng);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    updatePixmap();
}

void HeatmapDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    updatePixmap();
}

void HeatmapDialog::exportPng()
{
    const QString path = QFileDialog::getSaveFileName(this,
                                                      QStringLiteral("导出热力图"),
                                                      QStringLiteral("heatmap.png"),
                                                      QStringLiteral("PNG Images (*.png)"));
    if (path.isEmpty()) {
        return;
    }

    const QImage output = exportImage();
    if (output.save(path, "PNG")) {
        QMessageBox::information(this, QStringLiteral("导出成功"), QStringLiteral("热力图和色带已保存：%1").arg(path));
    } else {
        QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法保存热力图：%1").arg(path));
    }
}

void HeatmapDialog::updatePixmap()
{
    if (!imageLabel_ || image_.isNull()) {
        return;
    }

    const QSize targetSize = imageLabel_->contentsRect().size();
    if (!targetSize.isValid() || targetSize.isEmpty()) {
        return;
    }

    const QPixmap pixmap = QPixmap::fromImage(image_)
                               .scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    imageLabel_->setPixmap(pixmap);

    if (colorbarLabel_ && !colorbar_.isNull()) {
        const QPixmap barPixmap = QPixmap::fromImage(colorbar_)
                                      .scaled(colorbarLabel_->contentsRect().size(),
                                              Qt::IgnoreAspectRatio,
                                              Qt::SmoothTransformation);
        colorbarLabel_->setPixmap(barPixmap);
    }
}

QImage HeatmapDialog::exportImage() const
{
    const int width = 1100;
    const int height = 760;
    QImage output(width, height, QImage::Format_ARGB32);
    output.fill(QColor(245, 247, 250));

    QPainter painter(&output);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QColor(20, 24, 28));
    painter.setFont(QFont(QStringLiteral("Segoe UI"), 16, QFont::DemiBold));
    painter.drawText(QRect(30, 18, width - 60, 40), Qt::AlignLeft | Qt::AlignVCenter, windowTitle());

    const QRect imageRect(30, 76, 900, 640);
    painter.fillRect(imageRect, QColor(32, 37, 43));
    if (!image_.isNull()) {
        painter.drawImage(imageRect, image_.scaled(imageRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    painter.setPen(QPen(QColor(117, 128, 138), 2));
    painter.drawRect(imageRect);

    const QRect colorbarRect(970, 104, 42, 560);
    if (!colorbar_.isNull()) {
        painter.drawImage(colorbarRect, colorbar_.scaled(colorbarRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }
    painter.setPen(QPen(QColor(117, 128, 138), 1));
    painter.drawRect(colorbarRect);
    painter.setPen(QColor(20, 24, 28));
    painter.setFont(QFont(QStringLiteral("Segoe UI"), 11));
    painter.drawText(QRect(930, 74, 120, 24), Qt::AlignCenter, QString::number(vmax_, 'g', 6));
    painter.drawText(QRect(930, 668, 120, 24), Qt::AlignCenter, QString::number(vmin_, 'g', 6));

    return output;
}

} // namespace NFSScanner::UI
