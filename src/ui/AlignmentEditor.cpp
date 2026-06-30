#include "ui/AlignmentEditor.h"

#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace NFSScanner::UI {

AlignmentEditor::AlignmentEditor(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    auto *group = new QGroupBox(QStringLiteral("Alignment 矩形映射"), this);
    auto *form = new QFormLayout(group);

    auto makeSpin = [group](double value) {
        auto *spin = new QDoubleSpinBox(group);
        spin->setRange(-100000.0, 100000.0);
        spin->setDecimals(3);
        spin->setValue(value);
        return spin;
    };

    worldXMin_ = makeSpin(0.0);
    worldXMax_ = makeSpin(200.0);
    worldYMin_ = makeSpin(-300.0);
    worldYMax_ = makeSpin(0.0);
    pixelXMin_ = makeSpin(0.0);
    pixelXMax_ = makeSpin(640.0);
    pixelYMin_ = makeSpin(0.0);
    pixelYMax_ = makeSpin(480.0);

    form->addRow(QStringLiteral("World X min"), worldXMin_);
    form->addRow(QStringLiteral("World X max"), worldXMax_);
    form->addRow(QStringLiteral("World Y min"), worldYMin_);
    form->addRow(QStringLiteral("World Y max"), worldYMax_);
    form->addRow(QStringLiteral("Pixel X min"), pixelXMin_);
    form->addRow(QStringLiteral("Pixel X max"), pixelXMax_);
    form->addRow(QStringLiteral("Pixel Y min"), pixelYMin_);
    form->addRow(QStringLiteral("Pixel Y max"), pixelYMax_);

    backgroundLabel_ = new QLabel(QStringLiteral("背景图：未加载"), group);
    form->addRow(backgroundLabel_);

    auto *applyButton = new QPushButton(QStringLiteral("应用映射"), group);
    auto *backgroundButton = new QPushButton(QStringLiteral("加载背景图"), group);
    auto *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(applyButton);
    buttonRow->addWidget(backgroundButton);
    form->addRow(buttonRow);

    layout->addWidget(group);
    layout->addWidget(new QLabel(QStringLiteral("TODO(alignment): 多点透视标定。"), this));
    layout->addStretch(1);

    connect(applyButton, &QPushButton::clicked, this, &AlignmentEditor::syncFromUi);
    connect(backgroundButton, &QPushButton::clicked, this, &AlignmentEditor::loadBackgroundImage);
    syncFromUi();
}

NFSScanner::Core::AlignmentManager *AlignmentEditor::manager()
{
    return &manager_;
}

void AlignmentEditor::syncFromUi()
{
    NFSScanner::Core::AlignmentConfig config = manager_.config();
    config.enabled = true;
    config.worldXMin = worldXMin_->value();
    config.worldXMax = worldXMax_->value();
    config.worldYMin = worldYMin_->value();
    config.worldYMax = worldYMax_->value();
    config.pixelXMin = pixelXMin_->value();
    config.pixelXMax = pixelXMax_->value();
    config.pixelYMin = pixelYMin_->value();
    config.pixelYMax = pixelYMax_->value();
    manager_.setConfig(config);
    emit configApplied(config);
}

void AlignmentEditor::syncToUi()
{
    const NFSScanner::Core::AlignmentConfig config = manager_.config();
    worldXMin_->setValue(config.worldXMin);
    worldXMax_->setValue(config.worldXMax);
    worldYMin_->setValue(config.worldYMin);
    worldYMax_->setValue(config.worldYMax);
    pixelXMin_->setValue(config.pixelXMin);
    pixelXMax_->setValue(config.pixelXMax);
    pixelYMin_->setValue(config.pixelYMin);
    pixelYMax_->setValue(config.pixelYMax);
    if (config.backgroundImagePath.isEmpty()) {
        backgroundLabel_->setText(QStringLiteral("背景图：未加载"));
    } else {
        backgroundLabel_->setText(QStringLiteral("背景图：%1").arg(QFileInfo(config.backgroundImagePath).fileName()));
    }
}

void AlignmentEditor::loadBackgroundImage()
{
    const QString path = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("选择背景图"),
                                                      QString(),
                                                      QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty()) {
        return;
    }

    NFSScanner::Core::AlignmentConfig config = manager_.config();
    config.backgroundImagePath = path;
    manager_.setConfig(config);
    backgroundLabel_->setText(QStringLiteral("背景图：%1").arg(QFileInfo(path).fileName()));
    emit configApplied(config);
}

} // namespace NFSScanner::UI
