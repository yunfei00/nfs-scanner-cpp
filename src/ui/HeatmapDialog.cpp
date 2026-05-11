#include "ui/HeatmapDialog.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace NFSScanner::UI {

HeatmapDialog::HeatmapDialog(const QImage &image, const QString &title, QWidget *parent)
    : QDialog(parent)
    , image_(image)
{
    setWindowTitle(title);
    resize(800, 600);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    titleLabel_ = new QLabel(title, this);
    titleLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    titleLabel_->setWordWrap(true);
    layout->addWidget(titleLabel_);

    imageLabel_ = new QLabel(this);
    imageLabel_->setAlignment(Qt::AlignCenter);
    imageLabel_->setMinimumSize(480, 320);
    imageLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    imageLabel_->setStyleSheet(QStringLiteral("background:#20252b;border:1px solid #75808a;"));
    layout->addWidget(imageLabel_, 1);

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

    if (image_.save(path, "PNG")) {
        QMessageBox::information(this, QStringLiteral("导出成功"), QStringLiteral("热力图已保存：%1").arg(path));
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
}

} // namespace NFSScanner::UI
