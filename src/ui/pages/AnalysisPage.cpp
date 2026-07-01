#include "ui/pages/AnalysisPage.h"

#include "analysis/LutManager.h"
#include "project/ProjectManager.h"
#include "ui/HeatmapDialog.h"
#include "ui/HeatmapView.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace NFSScanner::UI {

AnalysisPage::AnalysisPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("analysisPage"));

    analysisController_ = new AnalysisController(this);
    connect(analysisController_, &AnalysisController::previewUpdated, this, [this]() {
        applyHeatmapPreviewToCanvases();
        updateColorbarDisplay();
        if (previewCanvas_) {
            HeatmapView::GridMapping mapping;
            mapping.xs = analysisController_->frequencyData().xs();
            mapping.ys = analysisController_->frequencyData().ys();
            const QVector<double> zs = analysisController_->frequencyData().zs();
            mapping.z = zs.isEmpty() ? 0.0 : zs.first();
            previewCanvas_->setGridMapping(mapping);
        }
    });
    connect(analysisController_, &AnalysisController::actualRangeUpdated, this, [this](double vmin, double vmax) {
        if (vminSpin_) {
            const QSignalBlocker blocker(vminSpin_);
            vminSpin_->setValue(vmin);
        }
        if (vmaxSpin_) {
            const QSignalBlocker blocker(vmaxSpin_);
            vmaxSpin_->setValue(vmax);
        }
    });

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    hintLabel_ = new QLabel(QStringLiteral("分析工作区：加载 traces.csv 后在此预览热力图。"), this);
    hintLabel_->setWordWrap(true);
    hintLabel_->setAlignment(Qt::AlignCenter);

    previewCanvas_ = new HeatmapView(this);
    previewCanvas_->setObjectName(QStringLiteral("analysisCanvasView"));
    previewCanvas_->setMinimumHeight(320);
    previewCanvas_->setCrosshairEnabled(true);
    connect(previewCanvas_, &HeatmapView::cursorSampleChanged,
            this, &AnalysisPage::updateHeatmapCursorReadout);

    layout->addWidget(hintLabel_, 0);
    layout->addWidget(previewCanvas_, 1);

    paramPanel_ = new QWidget(this);
    auto *paramLayout = new QVBoxLayout(paramPanel_);
    paramLayout->setContentsMargins(0, 0, 0, 0);
    paramLayout->addWidget(createResultGroup());
    paramLayout->addStretch(1);
}

HeatmapView *AnalysisPage::previewCanvas() const
{
    return previewCanvas_;
}

QLabel *AnalysisPage::hintLabel() const
{
    return hintLabel_;
}

QWidget *AnalysisPage::paramPanel() const
{
    return paramPanel_;
}

void AnalysisPage::bind(HeatmapView *scanCanvas,
                        NFSScanner::Project::ProjectManager *projectManager,
                        QWidget *messageBoxParent)
{
    scanCanvas_ = scanCanvas;
    projectManager_ = projectManager;
    messageBoxParent_ = messageBoxParent;

    if (projectManager_ && projectManager_->hasOpenProject() && resultDirEdit_) {
        resultDirEdit_->setText(projectManager_->defaultScanOutputDir());
    }
}

void AnalysisPage::refreshProjectPaths()
{
    if (!projectManager_ || !resultDirEdit_) {
        return;
    }
    resultDirEdit_->setText(projectManager_->defaultScanOutputDir());
}

void AnalysisPage::setResultDir(const QString &dir)
{
    if (resultDirEdit_) {
        resultDirEdit_->setText(dir);
    }
}

QString AnalysisPage::resultDir() const
{
    return resultDirEdit_ ? resultDirEdit_->text().trimmed() : QString();
}

QString AnalysisPage::currentTraceId() const
{
    return traceCombo_ ? traceCombo_->currentText() : QString();
}

QString AnalysisPage::currentLutName() const
{
    return lutCombo_ ? lutCombo_->currentText() : QStringLiteral("turbo");
}

double AnalysisPage::currentVmin() const
{
    return analysisController_ ? analysisController_->vmin() : 0.0;
}

double AnalysisPage::currentVmax() const
{
    return analysisController_ ? analysisController_->vmax() : 1.0;
}

QImage AnalysisPage::heatmapImage() const
{
    return analysisController_ ? analysisController_->heatmapImage() : QImage();
}

QGroupBox *AnalysisPage::createResultGroup()
{
    auto *group = new QGroupBox(QStringLiteral("结果区域"), paramPanel_);
    auto *layout = new QGridLayout(group);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(6);

    resultDirEdit_ = new QLineEdit(QStringLiteral("data/scans"), group);
    auto *viewButton = new QPushButton(QStringLiteral("查看"), group);
    auto *loadDataButton = new QPushButton(QStringLiteral("加载数据"), group);
    auto *heatmapButton = new QPushButton(QStringLiteral("显示热力图"), group);

    traceCombo_ = new QComboBox(group);
    frequencyCombo_ = new QComboBox(group);
    displayModeCombo_ = new QComboBox(group);
    displayModeCombo_->addItem(QStringLiteral("幅度"), QStringLiteral("magnitude"));
    displayModeCombo_->addItem(QStringLiteral("幅度dB"), QStringLiteral("db"));
    displayModeCombo_->addItem(QStringLiteral("相位"), QStringLiteral("phase"));
    displayModeCombo_->addItem(QStringLiteral("实部"), QStringLiteral("real"));
    displayModeCombo_->addItem(QStringLiteral("虚部"), QStringLiteral("imag"));

    lutCombo_ = new QComboBox(group);
    lutCombo_->addItems(Analysis::LutManager::availableLuts());
    lutCombo_->setCurrentText(QStringLiteral("turbo"));

    autoRangeCheck_ = new QCheckBox(QStringLiteral("自动范围"), group);
    autoRangeCheck_->setChecked(true);

    vminSpin_ = new QDoubleSpinBox(group);
    vminSpin_->setRange(-1e12, 1e12);
    vminSpin_->setDecimals(6);
    vminSpin_->setValue(0.0);
    vminSpin_->setEnabled(false);

    vmaxSpin_ = new QDoubleSpinBox(group);
    vmaxSpin_->setRange(-1e12, 1e12);
    vmaxSpin_->setDecimals(6);
    vmaxSpin_->setValue(1.0);
    vmaxSpin_->setEnabled(false);

    opacitySlider_ = new QSlider(Qt::Horizontal, group);
    opacitySlider_->setRange(0, 100);
    opacitySlider_->setValue(85);
    opacityLabel_ = new QLabel(group);
    updateOpacityLabel(opacitySlider_->value());

    auto *colorbarPanel = new QWidget(group);
    auto *colorbarLayout = new QVBoxLayout(colorbarPanel);
    colorbarLayout->setContentsMargins(0, 0, 0, 0);
    colorbarLayout->setSpacing(2);
    colorbarMaxLabel_ = new QLabel(QStringLiteral("1"), colorbarPanel);
    colorbarMaxLabel_->setAlignment(Qt::AlignCenter);
    colorbarLabel_ = new QLabel(colorbarPanel);
    colorbarLabel_->setAlignment(Qt::AlignCenter);
    colorbarLabel_->setFixedSize(36, 88);
    colorbarLabel_->setStyleSheet(QStringLiteral("background:#20252b;border:1px solid #75808a;"));
    colorbarMinLabel_ = new QLabel(QStringLiteral("0"), colorbarPanel);
    colorbarMinLabel_->setAlignment(Qt::AlignCenter);
    colorbarLayout->addWidget(colorbarMaxLabel_);
    colorbarLayout->addWidget(colorbarLabel_);
    colorbarLayout->addWidget(colorbarMinLabel_);

    layout->addWidget(new QLabel(QStringLiteral("结果"), group), 0, 0);
    layout->addWidget(resultDirEdit_, 0, 1, 1, 5);
    layout->addWidget(viewButton, 0, 6);
    layout->addWidget(loadDataButton, 0, 7);
    layout->addWidget(new QLabel(QStringLiteral("Trace"), group), 1, 0);
    layout->addWidget(traceCombo_, 1, 1);
    layout->addWidget(new QLabel(QStringLiteral("Frequency"), group), 1, 2);
    layout->addWidget(frequencyCombo_, 1, 3);
    layout->addWidget(new QLabel(QStringLiteral("显示模式"), group), 1, 4);
    layout->addWidget(displayModeCombo_, 1, 5);
    layout->addWidget(heatmapButton, 1, 6, 1, 2);
    layout->addWidget(new QLabel(QStringLiteral("LUT"), group), 2, 0);
    layout->addWidget(lutCombo_, 2, 1);
    layout->addWidget(autoRangeCheck_, 2, 2);
    layout->addWidget(new QLabel(QStringLiteral("vmin"), group), 2, 3);
    layout->addWidget(vminSpin_, 2, 4);
    layout->addWidget(new QLabel(QStringLiteral("vmax"), group), 2, 5);
    layout->addWidget(vmaxSpin_, 2, 6);
    layout->addWidget(colorbarPanel, 2, 7, 2, 1);
    layout->addWidget(opacityLabel_, 3, 0);
    layout->addWidget(opacitySlider_, 3, 1, 1, 6);
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(3, 1);

    connect(viewButton, &QPushButton::clicked, this, [this]() {
        const QString path = resultDir();
        const QFileInfo info(path);
        if (path.isEmpty() || !info.exists() || !info.isDir()) {
            emit logMessage(QStringLiteral("结果目录不存在：%1").arg(path.isEmpty() ? QStringLiteral("(空)") : path));
            return;
        }

        emit logMessage(QStringLiteral("打开结果目录：%1").arg(info.absoluteFilePath()));
        QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath()));
    });
    connect(loadDataButton, &QPushButton::clicked, this, &AnalysisPage::loadFrequencyData);
    connect(heatmapButton, &QPushButton::clicked, this, &AnalysisPage::showHeatmap);
    connect(autoRangeCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        if (vminSpin_) {
            vminSpin_->setEnabled(!checked);
        }
        if (vmaxSpin_) {
            vmaxSpin_->setEnabled(!checked);
        }
        scheduleHeatmapPreviewRefresh();
    });
    connect(opacitySlider_, &QSlider::valueChanged, this, &AnalysisPage::updateOpacityLabel);
    connect(lutCombo_, &QComboBox::currentTextChanged, this, [this]() {
        if (!analysisController_ || analysisController_->colorbarImage().isNull()) {
            updateColorbarDisplay();
        }
        scheduleHeatmapPreviewRefresh();
    });
    connect(opacitySlider_, &QSlider::valueChanged, this, [this]() {
        if (!analysisController_ || analysisController_->colorbarImage().isNull()) {
            updateColorbarDisplay();
        }
    });
    connect(traceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalysisPage::scheduleHeatmapPreviewRefresh);
    connect(frequencyCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalysisPage::scheduleHeatmapPreviewRefresh);
    connect(displayModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnalysisPage::scheduleHeatmapPreviewRefresh);
    connect(vminSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnalysisPage::scheduleHeatmapPreviewRefresh);
    connect(vmaxSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnalysisPage::scheduleHeatmapPreviewRefresh);

    updateColorbarDisplay();

    return group;
}

void AnalysisPage::loadFrequencyData()
{
    const QString tracePath = resolveTraceCsvPath();
    if (tracePath.isEmpty()) {
        const QString message = QStringLiteral("traces.csv 不存在，请选择任务目录或 CSV 文件。");
        emit logMessage(message);
        QMessageBox::warning(messageBoxParent_, QStringLiteral("加载失败"), message);
        return;
    }

    QString errorMessage;
    if (!analysisController_ || !analysisController_->loadTraceCsv(tracePath, &errorMessage)) {
        emit logMessage(QStringLiteral("加载数据失败：%1").arg(errorMessage));
        QMessageBox::warning(messageBoxParent_, QStringLiteral("加载失败"), errorMessage);
        return;
    }

    populateFrequencyControls();
    const auto &data = analysisController_->frequencyData();
    emit logMessage(QStringLiteral("已加载数据：trace数量=%1，频率点=%2，坐标点=%3")
                        .arg(data.traceIds().size())
                        .arg(data.frequencyCount())
                        .arg(data.pointCount()));
    scheduleHeatmapPreviewRefresh();
}

void AnalysisPage::populateFrequencyControls()
{
    if (!analysisController_) {
        return;
    }
    const auto &data = analysisController_->frequencyData();
    if (traceCombo_) {
        traceCombo_->clear();
        traceCombo_->addItems(data.traceIds());
    }

    if (frequencyCombo_) {
        frequencyCombo_->clear();
        const QVector<double> freqs = data.freqs();
        for (double freq : freqs) {
            frequencyCombo_->addItem(formatFrequency(freq), freq);
        }
    }
}

AnalysisRenderParams AnalysisPage::buildAnalysisParams() const
{
    AnalysisRenderParams params;
    params.traceId = traceCombo_ ? traceCombo_->currentText() : QString();
    params.freqIndex = frequencyCombo_ ? frequencyCombo_->currentIndex() : -1;
    params.mode = selectedDisplayMode();
    params.lutName = lutCombo_ ? lutCombo_->currentText() : QStringLiteral("turbo");
    params.autoRange = !autoRangeCheck_ || autoRangeCheck_->isChecked();
    params.vmin = vminSpin_ ? vminSpin_->value() : 0.0;
    params.vmax = vmaxSpin_ ? vmaxSpin_->value() : 1.0;
    params.opacityPercent = opacitySlider_ ? opacitySlider_->value() : 85;
    return params;
}

void AnalysisPage::showHeatmap()
{
    if (!analysisController_ || !analysisController_->frequencyData().isValid()) {
        const QString message = QStringLiteral("请先加载 traces.csv 数据。");
        emit logMessage(message);
        QMessageBox::warning(messageBoxParent_, QStringLiteral("无法显示热力图"), message);
        return;
    }

    const QString traceId = traceCombo_ ? traceCombo_->currentText() : QString();
    if (traceId.isEmpty()) {
        const QString message = QStringLiteral("未选择 Trace。");
        emit logMessage(message);
        QMessageBox::warning(messageBoxParent_, QStringLiteral("无法显示热力图"), message);
        return;
    }

    if (!refreshHeatmapPreview()) {
        const QString message = QStringLiteral("生成热力图失败，请检查 Trace/频率与数据范围。");
        emit logMessage(message);
        QMessageBox::warning(messageBoxParent_, QStringLiteral("无法显示热力图"), message);
        return;
    }

    const QString title = QStringLiteral("%1 | %2 | %3")
                              .arg(traceId,
                                   frequencyCombo_ ? frequencyCombo_->currentText() : QStringLiteral("Frequency"),
                                   displayModeCombo_ ? displayModeCombo_->currentText() : QStringLiteral("幅度"));
    emit logMessage(QStringLiteral("热力图已生成：LUT=%1，范围=%2 ~ %3，透明度=%4%")
                        .arg(lutCombo_ ? lutCombo_->currentText() : QStringLiteral("turbo"),
                             QString::number(analysisController_->vmin(), 'g', 6),
                             QString::number(analysisController_->vmax(), 'g', 6),
                             QString::number(opacitySlider_ ? opacitySlider_->value() : 85)));

    HeatmapDialog dialog(analysisController_->heatmapImage(),
                         analysisController_->colorbarImage(),
                         title,
                         analysisController_->vmin(),
                         analysisController_->vmax(),
                         messageBoxParent_);
    dialog.exec();
}

void AnalysisPage::exportAnalysisConfigJson()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("trace"), traceCombo_ ? traceCombo_->currentText() : QString());
    obj.insert(QStringLiteral("frequency_hz"), frequencyCombo_ ? frequencyCombo_->currentData().toDouble() : 0.0);
    obj.insert(QStringLiteral("display_mode"), selectedDisplayMode());
    obj.insert(QStringLiteral("lut"), lutCombo_ ? lutCombo_->currentText() : QStringLiteral("turbo"));
    obj.insert(QStringLiteral("vmin"), analysisController_ ? analysisController_->vmin() : 0.0);
    obj.insert(QStringLiteral("vmax"), analysisController_ ? analysisController_->vmax() : 1.0);
    obj.insert(QStringLiteral("opacity"), opacitySlider_ ? opacitySlider_->value() : 85);

    const QString path = QFileDialog::getSaveFileName(messageBoxParent_, QStringLiteral("导出分析配置"),
                                                      QStringLiteral("analysis_config.json"),
                                                      QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(messageBoxParent_, QStringLiteral("导出失败"), file.errorString());
        return;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    emit logMessage(QStringLiteral("分析配置已导出：%1").arg(path));
}

void AnalysisPage::scheduleHeatmapPreviewRefresh()
{
    if (!analysisController_ || !analysisController_->frequencyData().isValid()) {
        return;
    }
    analysisController_->scheduleRefresh(buildAnalysisParams());
}

bool AnalysisPage::refreshHeatmapPreview()
{
    if (!analysisController_) {
        return false;
    }
    return analysisController_->refreshPreview(buildAnalysisParams(), nullptr);
}

void AnalysisPage::applyHeatmapPreviewToCanvases()
{
    if (!analysisController_) {
        return;
    }
    const int opacity = opacitySlider_ ? opacitySlider_->value() : 85;
    const QImage heatmap = analysisController_->heatmapImage();
    if (scanCanvas_) {
        scanCanvas_->setOpacityPercent(opacity);
        scanCanvas_->setHeatmapImage(heatmap);
    }
    if (previewCanvas_) {
        previewCanvas_->setOpacityPercent(opacity);
        previewCanvas_->setHeatmapImage(heatmap);
    }
}

void AnalysisPage::updateHeatmapCursorReadout(double worldX, double worldY, bool insideImage)
{
    if (!hintLabel_) {
        return;
    }

    if (!insideImage || !analysisController_ || !analysisController_->frequencyData().isValid()) {
        hintLabel_->setText(
            QStringLiteral("分析工作区：加载 traces.csv 后在此预览热力图。移动鼠标查看坐标与读数。"));
        return;
    }

    const QString traceId = traceCombo_ ? traceCombo_->currentText() : QString();
    const int freqIndex = frequencyCombo_ ? frequencyCombo_->currentIndex() : -1;
    const auto &frequencyData = analysisController_->frequencyData();
    const QVector<double> zs = frequencyData.zs();
    const double z = zs.isEmpty() ? 0.0 : zs.first();
    const QString mode = selectedDisplayMode();

    QString valueText = QStringLiteral("—");
    if (!traceId.isEmpty() && freqIndex >= 0 && frequencyData.hasValue(worldX, worldY, z, traceId)) {
        const double value = frequencyData.scalarValue(worldX, worldY, z, traceId, freqIndex, mode);
        valueText = QString::number(value, 'g', 6);
    }

    hintLabel_->setText(
        QStringLiteral("光标：X=%1 mm  Y=%2 mm  Z=%3 mm  |  %4 = %5")
            .arg(QString::number(worldX, 'f', 2),
                 QString::number(worldY, 'f', 2),
                 QString::number(z, 'f', 2),
                 displayModeCombo_ ? displayModeCombo_->currentText() : QStringLiteral("值"),
                 valueText));
}

void AnalysisPage::updateColorbarDisplay()
{
    if (!colorbarLabel_) {
        return;
    }

    const QString lutName = lutCombo_ ? lutCombo_->currentText() : QStringLiteral("turbo");
    const int alpha = opacitySlider_ ? std::clamp(opacitySlider_->value() * 255 / 100, 0, 255) : 220;
    const QImage colorbar = analysisController_ ? analysisController_->colorbarImage() : QImage();
    const QImage source = colorbar.isNull()
        ? Analysis::LutManager::createColorbar(lutName, 28, 120, alpha)
        : colorbar;

    colorbarLabel_->setPixmap(QPixmap::fromImage(source)
                                  .scaled(colorbarLabel_->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

    if (colorbarMinLabel_) {
        colorbarMinLabel_->setText(QString::number(analysisController_ ? analysisController_->vmin() : 0.0, 'g', 6));
    }
    if (colorbarMaxLabel_) {
        colorbarMaxLabel_->setText(QString::number(analysisController_ ? analysisController_->vmax() : 1.0, 'g', 6));
    }
}

void AnalysisPage::updateOpacityLabel(int percent)
{
    const int safePercent = std::clamp(percent, 0, 100);
    if (opacityLabel_) {
        opacityLabel_->setText(QStringLiteral("透明度 %1%").arg(safePercent));
    }
    if (scanCanvas_) {
        scanCanvas_->setOpacityPercent(safePercent);
    }
    if (previewCanvas_) {
        previewCanvas_->setOpacityPercent(safePercent);
    }
}

QString AnalysisPage::selectedDisplayMode() const
{
    if (!displayModeCombo_) {
        return QStringLiteral("magnitude");
    }

    const QString mode = displayModeCombo_->currentData().toString();
    return mode.isEmpty() ? QStringLiteral("magnitude") : mode;
}

QString AnalysisPage::formatFrequency(double hz) const
{
    const double absHz = std::abs(hz);
    if (absHz >= 1e9) {
        return QStringLiteral("%1 GHz").arg(QString::number(hz / 1e9, 'f', 6));
    }
    if (absHz >= 1e6) {
        return QStringLiteral("%1 MHz").arg(QString::number(hz / 1e6, 'f', 6));
    }
    if (absHz >= 1e3) {
        return QStringLiteral("%1 kHz").arg(QString::number(hz / 1e3, 'f', 3));
    }
    return QStringLiteral("%1 Hz").arg(QString::number(hz, 'f', 0));
}

QString AnalysisPage::resolveTraceCsvPath() const
{
    const QString path = resultDir();
    if (path.isEmpty()) {
        return {};
    }

    const QFileInfo info(path);
    if (info.exists() && info.isFile() && info.fileName().compare(QStringLiteral("traces.csv"), Qt::CaseInsensitive) == 0) {
        return info.absoluteFilePath();
    }

    if (info.exists() && info.isDir()) {
        const QFileInfo csvInfo(QDir(info.absoluteFilePath()).filePath(QStringLiteral("traces.csv")));
        if (csvInfo.exists() && csvInfo.isFile()) {
            return csvInfo.absoluteFilePath();
        }
    }

    return {};
}

} // namespace NFSScanner::UI
