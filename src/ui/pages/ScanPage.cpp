#include "ui/pages/ScanPage.h"

#include "core/AlignmentManager.h"
#include "core/ScanManager.h"
#include "core/ScanPathPlanner.h"
#include "project/ProjectManager.h"
#include "ui/AlignmentEditor.h"
#include "ui/HeatmapView.h"
#include "ui/UiFormUtils.h"

#include <QAbstractScrollArea>
#include <QCheckBox>
#include <QDir>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace NFSScanner::UI {

namespace {

QString mmText(double value)
{
    return QString::number(value, 'f', 2);
}

QString positionText(double x, double y, double z)
{
    return QStringLiteral("X=%1 Y=%2 Z=%3")
        .arg(mmText(x), mmText(y), mmText(z));
}

} // namespace

ScanPage::ScanPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("scanPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    canvas_ = new HeatmapView(this);
    canvas_->setObjectName(QStringLiteral("scanCanvasView"));
    layout->addWidget(canvas_, 1);

    paramPanel_ = buildParamPanel();

    mockScanTimer_ = new QTimer(this);
    mockScanTimer_->setInterval(100);
    connect(mockScanTimer_, &QTimer::timeout, this, &ScanPage::advanceMockScan);
}

HeatmapView *ScanPage::canvas() const
{
    return canvas_;
}

AlignmentEditor *ScanPage::alignmentEditor() const
{
    return alignmentEditor_;
}

QWidget *ScanPage::paramPanel() const
{
    return paramPanel_;
}

void ScanPage::bind(NFSScanner::Core::ScanManager *scanManager,
                    HeatmapView *heatmapView,
                    AlignmentEditor *alignmentEditor,
                    NFSScanner::Core::AlignmentManager *alignmentManager,
                    NFSScanner::Project::ProjectManager *projectManager,
                    QPlainTextEdit *logEdit,
                    QWidget *messageBoxParent)
{
    scanManager_ = scanManager;
    heatmapView_ = heatmapView;
    alignmentEditor_ = alignmentEditor;
    alignmentManager_ = alignmentManager;
    projectManager_ = projectManager;
    logEdit_ = logEdit;
    messageBoxParent_ = messageBoxParent;

    if (alignmentEditor_) {
        connect(alignmentEditor_, &AlignmentEditor::configApplied, this, [this](const Core::AlignmentConfig &config) {
            if (alignmentManager_) {
                alignmentManager_->setConfig(config);
            }
            applyAlignmentToHeatmapView(config);
        });
    }

    if (scanManager_) {
        connect(scanManager_, &Core::ScanManager::stateChanged, this, [this](const QString &) {
            updateActionButtons();
        });
        connect(scanManager_, &Core::ScanManager::taskDirChanged, this, [this](const QString &taskDir) {
            saveAlignmentForTaskDir(taskDir, readScanConfigFromUi());
        });
        connect(scanManager_, &Core::ScanManager::scanFinished, this, [this]() {
            updateActionButtons();
        });
        connect(scanManager_, &Core::ScanManager::scanError, this, [this](const QString &) {
            updateActionButtons();
        });
    }

    updateActionButtons();
}

void ScanPage::setFeedProvider(FeedProvider provider)
{
    feedProvider_ = std::move(provider);
}

void ScanPage::setMockModeChecker(BoolProvider checker)
{
    mockModeChecker_ = std::move(checker);
}

void ScanPage::setMotionReadyChecker(BoolProvider checker)
{
    motionReadyChecker_ = std::move(checker);
}

void ScanPage::setScanLaunchHandler(ScanLaunchHandler handler)
{
    scanLaunchHandler_ = std::move(handler);
}

void ScanPage::setOnScanPageChecker(PageChecker checker)
{
    onScanPageChecker_ = std::move(checker);
}

void ScanPage::setResultDirProvider(StringProvider provider)
{
    resultDirProvider_ = std::move(provider);
}

void ScanPage::setCurrentPosition(double x, double y, double z)
{
    currentX_ = x;
    currentY_ = y;
    currentZ_ = z;
}

QWidget *ScanPage::buildParamPanel()
{
    auto *panel = new QWidget;
    panel->setObjectName(QStringLiteral("scanParamPanel"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(6);
    layout->addWidget(createScanAreaGroup());
    layout->addWidget(createTestInfoGroup());
    layout->addWidget(createStepConfigGroup());
    layout->addWidget(createActionGroup());

    auto *editor = new AlignmentEditor(panel);
    editor->setObjectName(QStringLiteral("alignmentEditor"));
    alignmentEditor_ = editor;
    layout->addWidget(editor);
    layout->addStretch(1);
    return panel;
}

QGroupBox *ScanPage::createScanAreaGroup()
{
    auto *group = new QGroupBox(QStringLiteral("扫描区域"));
    group->setMaximumHeight(170);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 12, 10, 10);

    scanTable_ = new QTableWidget(1, 9, group);
    scanTable_->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("start_x"),
        QStringLiteral("start_y"),
        QStringLiteral("start_z"),
        QStringLiteral("end_x"),
        QStringLiteral("end_y"),
        QStringLiteral("end_z"),
        QStringLiteral("step_x"),
        QStringLiteral("step_y"),
        QStringLiteral("step_z"),
    });
    scanTable_->verticalHeader()->hide();
    scanTable_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scanTable_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scanTable_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    scanTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    scanTable_->horizontalHeader()->setStretchLastSection(false);
    scanTable_->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    scanTable_->setAlternatingRowColors(true);
    scanTable_->setMaximumHeight(82);
    scanTable_->setMinimumHeight(78);

    const QList<double> defaults{0.0, 0.0, 1.0, 10.0, 10.0, 1.0, 1.0, 1.0, 1.0};
    for (int column = 0; column < defaults.size(); ++column) {
        setScanTableValue(column, defaults.at(column));
    }

    layout->addWidget(scanTable_);

    auto *scanOptionRow = new QWidget(group);
    auto *optionLayout = new QHBoxLayout(scanOptionRow);
    optionLayout->setContentsMargins(0, 0, 0, 0);
    optionLayout->setSpacing(8);

    snakeModeCheck_ = new QCheckBox(QStringLiteral("蛇形扫描"), scanOptionRow);
    snakeModeCheck_->setChecked(true);

    dwellTimeSpinBox_ = new QSpinBox(scanOptionRow);
    dwellTimeSpinBox_->setRange(50, 10000);
    dwellTimeSpinBox_->setValue(100);
    dwellTimeSpinBox_->setSuffix(QStringLiteral(" ms"));
    dwellTimeSpinBox_->setMaximumWidth(110);

    optionLayout->addWidget(snakeModeCheck_);
    optionLayout->addStretch(1);
    optionLayout->addWidget(new QLabel(QStringLiteral("驻留时间"), scanOptionRow));
    optionLayout->addWidget(dwellTimeSpinBox_);
    layout->addWidget(scanOptionRow);

    return group;
}

QGroupBox *ScanPage::createTestInfoGroup()
{
    auto *group = new QGroupBox(QStringLiteral("测试说明"));
    auto *layout = new QFormLayout(group);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setSpacing(7);

    projectNameEdit_ = new QLineEdit(group);
    projectNameEdit_->setPlaceholderText(QStringLiteral("请输入项目名称"));
    testNameEdit_ = new QLineEdit(group);
    testNameEdit_->setPlaceholderText(QStringLiteral("请输入测试名称"));

    layout->addRow(QStringLiteral("项目名称"), projectNameEdit_);
    layout->addRow(QStringLiteral("测试名称"), testNameEdit_);

    return group;
}

QGroupBox *ScanPage::createStepConfigGroup()
{
    auto *group = new QGroupBox(QStringLiteral("步长设置"));
    auto *layout = new QGridLayout(group);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(6);

    stepXEdit_ = createDoubleEdit(QStringLiteral("0.50"), group);
    stepYEdit_ = createDoubleEdit(QStringLiteral("0.50"), group);
    stepZEdit_ = createDoubleEdit(QStringLiteral("0.50"), group);
    auto *setStartButton = new QPushButton(QStringLiteral("设为起点"), group);
    auto *setEndButton = new QPushButton(QStringLiteral("设为终点"), group);

    layout->addWidget(new QLabel(QStringLiteral("StepX"), group), 0, 0);
    layout->addWidget(stepXEdit_, 0, 1);
    layout->addWidget(new QLabel(QStringLiteral("mm"), group), 0, 2);
    layout->addWidget(new QLabel(QStringLiteral("StepY"), group), 1, 0);
    layout->addWidget(stepYEdit_, 1, 1);
    layout->addWidget(new QLabel(QStringLiteral("mm"), group), 1, 2);
    layout->addWidget(new QLabel(QStringLiteral("StepZ"), group), 2, 0);
    layout->addWidget(stepZEdit_, 2, 1);
    layout->addWidget(new QLabel(QStringLiteral("mm"), group), 2, 2);
    layout->addWidget(setStartButton, 3, 0, 1, 2);
    layout->addWidget(setEndButton, 3, 2);

    connect(stepXEdit_, &QLineEdit::editingFinished, this, &ScanPage::syncStepInputsToTable);
    connect(stepYEdit_, &QLineEdit::editingFinished, this, &ScanPage::syncStepInputsToTable);
    connect(stepZEdit_, &QLineEdit::editingFinished, this, &ScanPage::syncStepInputsToTable);
    connect(setStartButton, &QPushButton::clicked, this, [this]() {
        setCurrentPositionAsScanPoint(true);
    });
    connect(setEndButton, &QPushButton::clicked, this, [this]() {
        setCurrentPositionAsScanPoint(false);
    });

    return group;
}

QGroupBox *ScanPage::createActionGroup()
{
    auto *group = new QGroupBox(QStringLiteral("功能操作区"));
    auto *layout = new QGridLayout(group);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setHorizontalSpacing(7);
    layout->setVerticalSpacing(7);

    startScanButton_ = new QPushButton(QStringLiteral("开始"), group);
    startScanButton_->setObjectName(QStringLiteral("primaryButton"));
    pauseScanButton_ = new QPushButton(QStringLiteral("暂停"), group);
    stopScanButton_ = new QPushButton(QStringLiteral("停止"), group);

    layout->addWidget(startScanButton_, 0, 0);
    layout->addWidget(pauseScanButton_, 0, 1);
    layout->addWidget(stopScanButton_, 0, 2);

    auto *previewPathButton = new QPushButton(QStringLiteral("预览路径"), group);
    layout->addWidget(previewPathButton, 1, 0, 1, 3);

    connect(previewPathButton, &QPushButton::clicked, this, &ScanPage::previewScanPath);

    scanProgressBar_ = new QProgressBar(group);
    scanProgressBar_->setRange(0, 1);
    scanProgressBar_->setValue(0);
    scanProgressBar_->setTextVisible(true);
    layout->addWidget(scanProgressBar_, 2, 0, 1, 3);

    connect(startScanButton_, &QPushButton::clicked, this, &ScanPage::startScan);
    connect(pauseScanButton_, &QPushButton::clicked, this, &ScanPage::pauseScan);
    connect(stopScanButton_, &QPushButton::clicked, this, &ScanPage::stopScan);

    return group;
}

void ScanPage::appendLog(const QString &text)
{
    emit logMessage(text);
}

void ScanPage::setCurrentPositionAsScanPoint(bool startPoint)
{
    syncStepInputsToTable();
    const int offset = startPoint ? 0 : 3;
    setScanTableValue(offset, currentX_);
    setScanTableValue(offset + 1, currentY_);
    setScanTableValue(offset + 2, currentZ_);
    appendLog(QStringLiteral("已将当前位置写入扫描%1：%2")
                  .arg(startPoint ? QStringLiteral("起点") : QStringLiteral("终点"),
                       positionText(currentX_, currentY_, currentZ_)));
}

void ScanPage::syncStepInputsToTable()
{
    if (!scanTable_) {
        return;
    }

    bool xOk = false;
    bool yOk = false;
    bool zOk = false;
    const double x = stepXEdit_->text().toDouble(&xOk);
    const double y = stepYEdit_->text().toDouble(&yOk);
    const double z = stepZEdit_->text().toDouble(&zOk);
    if (xOk) {
        setScanTableValue(6, x);
    }
    if (yOk) {
        setScanTableValue(7, y);
    }
    if (zOk) {
        setScanTableValue(8, z);
    }
}

void ScanPage::startScan()
{
    if (!scanManager_) {
        return;
    }

    if (onScanPageChecker_ && !onScanPageChecker_()) {
        appendLog(QStringLiteral("请切换到扫描页后开始扫描。"));
        emit requestSwitchToScanPage();
        return;
    }

    syncStepInputsToTable();
    const Core::ScanConfig config = readScanConfigFromUi();

    Core::ScanPathPlanner planner;
    const QVector<Core::ScanPoint> previewPoints = planner.generate(config);
    if (previewPoints.isEmpty()) {
        const QString message = planner.lastError().isEmpty()
            ? QStringLiteral("扫描路径生成失败，请检查参数。")
            : planner.lastError();
        appendLog(message);
        QMessageBox::warning(messageBoxParent_, QStringLiteral("无法开始扫描"), message);
        return;
    }
    applyPathPreviewToCanvas(config, previewPoints);
    appendLog(QStringLiteral("扫描路径：共 %1 个点（蛇形：%2）")
                  .arg(previewPoints.size())
                  .arg(config.snakeMode ? QStringLiteral("是") : QStringLiteral("否")));

    const bool mockMode = !mockModeChecker_ || mockModeChecker_();
    const bool useRealMotion = !mockMode;
    if (useRealMotion && motionReadyChecker_ && !motionReadyChecker_()) {
        const QString message = QStringLiteral("请先打开运动控制串口，或勾选模拟模式。");
        appendLog(message);
        QMessageBox::warning(messageBoxParent_, QStringLiteral("运动控制未连接"), message);
        return;
    }

    updateScanProgress(0, 1);

    if (scanLaunchHandler_ && !scanLaunchHandler_(scanManager_, config)) {
        return;
    }

    scanManager_->startScan(config);
    updateActionButtons();
}

void ScanPage::pauseScan()
{
    if (!scanManager_) {
        return;
    }

    if (scanManager_->state() == Core::ScanState::Running) {
        scanManager_->pauseScan();
    } else if (scanManager_->state() == Core::ScanState::Paused) {
        scanManager_->resumeScan();
    }
    updateActionButtons();
}

void ScanPage::stopScan()
{
    if (!scanManager_) {
        return;
    }

    scanManager_->stopScan();
    updateActionButtons();
}

void ScanPage::advanceMockScan()
{
    if (scanIndex_ >= mockScanPoints_.size()) {
        finishMockScan();
        return;
    }

    const MockScanPoint point = mockScanPoints_.at(scanIndex_);
    ++scanIndex_;
    currentX_ = point.x;
    currentY_ = point.y;
    currentZ_ = point.z;

    const int total = mockScanPoints_.size();
    const int remaining = std::max(0, total - scanIndex_);
    const int estimatedSeconds = static_cast<int>(std::ceil(remaining / 10.0));
    emit mockScanPositionChanged(currentX_, currentY_, currentZ_);
    emit mockScanProgressChanged(remaining, estimatedSeconds);
    emit mockScanStateChanged(QStringLiteral("扫描中"));
    appendLog(QStringLiteral("扫描点 %1/%2：X=%3 Y=%4 Z=%5")
                  .arg(scanIndex_)
                  .arg(total)
                  .arg(mmText(currentX_),
                       mmText(currentY_),
                       mmText(currentZ_)));

    if (scanIndex_ >= total) {
        finishMockScan();
    }
}

void ScanPage::finishMockScan()
{
    if (mockScanTimer_->isActive()) {
        mockScanTimer_->stop();
    }
    emit mockScanProgressChanged(0, 0);
    emit mockScanStateChanged(QStringLiteral("已完成"));
    appendLog(QStringLiteral("扫描完成。"));
    emit mockScanFinished();
    updateActionButtons();
}

void ScanPage::updateActionButtons()
{
    if (!startScanButton_ || !pauseScanButton_ || !stopScanButton_) {
        return;
    }

    const Core::ScanState state = scanManager_ ? scanManager_->state() : Core::ScanState::Idle;
    const bool preparing = state == Core::ScanState::Preparing;
    const bool running = state == Core::ScanState::Running;
    const bool paused = state == Core::ScanState::Paused;
    const bool stopping = state == Core::ScanState::Stopping;

    startScanButton_->setText(QStringLiteral("开始"));
    startScanButton_->setEnabled(!preparing && !running && !paused && !stopping);
    pauseScanButton_->setText(paused ? QStringLiteral("继续") : QStringLiteral("暂停"));
    pauseScanButton_->setEnabled(running || paused);
    stopScanButton_->setEnabled(preparing || running || paused);
    setScanParamsLocked(preparing || running || paused || stopping);
}

Core::ScanConfig ScanPage::readScanConfigFromUi() const
{
    Core::ScanConfig config;
    config.startX = scanTableValue(0, 0.0);
    config.startY = scanTableValue(1, 0.0);
    config.startZ = scanTableValue(2, 1.0);
    config.endX = scanTableValue(3, 10.0);
    config.endY = scanTableValue(4, 10.0);
    config.endZ = scanTableValue(5, 1.0);
    config.stepX = scanTableValue(6, 1.0);
    config.stepY = scanTableValue(7, 1.0);
    config.stepZ = scanTableValue(8, 1.0);
    config.feed = feedProvider_ ? feedProvider_() : 1000.0;
    config.dwellMs = dwellTimeSpinBox_ ? dwellTimeSpinBox_->value() : 100;
    config.snakeMode = !snakeModeCheck_ || snakeModeCheck_->isChecked();
    config.projectName = projectNameEdit_ ? projectNameEdit_->text().trimmed() : QString();
    config.testName = testNameEdit_ ? testNameEdit_->text().trimmed() : QString();

    if (projectManager_ && projectManager_->hasOpenProject()) {
        config.outputDir = projectManager_->defaultScanOutputDir();
    } else if (resultDirProvider_) {
        const QString dir = resultDirProvider_().trimmed();
        config.outputDir = dir.isEmpty() ? QStringLiteral("data/scans") : dir;
    } else {
        config.outputDir = QStringLiteral("data/scans");
    }
    return config;
}

void ScanPage::setScanParamsLocked(bool locked)
{
    if (scanTable_) {
        scanTable_->setEnabled(!locked);
    }
    if (stepXEdit_) {
        stepXEdit_->setEnabled(!locked);
    }
    if (stepYEdit_) {
        stepYEdit_->setEnabled(!locked);
    }
    if (stepZEdit_) {
        stepZEdit_->setEnabled(!locked);
    }
    if (snakeModeCheck_) {
        snakeModeCheck_->setEnabled(!locked);
    }
    if (dwellTimeSpinBox_) {
        dwellTimeSpinBox_->setEnabled(!locked);
    }
    if (projectNameEdit_) {
        projectNameEdit_->setEnabled(!locked);
    }
    if (testNameEdit_) {
        testNameEdit_->setEnabled(!locked);
    }
}

void ScanPage::previewScanPath()
{
    syncStepInputsToTable();
    const Core::ScanConfig config = readScanConfigFromUi();
    Core::ScanPathPlanner planner;
    const QVector<Core::ScanPoint> points = planner.generate(config);
    if (points.isEmpty()) {
        const QString message = planner.lastError().isEmpty()
            ? QStringLiteral("路径预览失败。")
            : planner.lastError();
        appendLog(message);
        QMessageBox::warning(messageBoxParent_, QStringLiteral("路径预览"), message);
        return;
    }
    applyPathPreviewToCanvas(config, points);
    appendLog(QStringLiteral("路径预览：%1 个点。").arg(points.size()));
}

void ScanPage::applyPathPreviewToCanvas(const Core::ScanConfig &config, const QVector<Core::ScanPoint> &points)
{
    if (!heatmapView_) {
        return;
    }

    const double xMin = std::min(config.startX, config.endX);
    const double xMax = std::max(config.startX, config.endX);
    const double yMin = std::min(config.startY, config.endY);
    const double yMax = std::max(config.startY, config.endY);

    QVector<QPointF> path;
    path.reserve(points.size());
    for (const Core::ScanPoint &point : points) {
        path.append(QPointF(point.x, point.y));
    }
    heatmapView_->setScanRegionOverlay(xMin, yMin, xMax, yMax, path);
    heatmapView_->setScanProgress(0, points.size());
}

void ScanPage::applyAlignmentToHeatmapView(const Core::AlignmentConfig &config)
{
    if (!heatmapView_) {
        return;
    }
    if (config.backgroundImagePath.isEmpty()) {
        return;
    }
    const QImage background(config.backgroundImagePath);
    if (background.isNull()) {
        appendLog(QStringLiteral("背景图加载失败：%1").arg(config.backgroundImagePath));
        return;
    }
    heatmapView_->setBackgroundImage(background);
}

void ScanPage::saveAlignmentForTaskDir(const QString &taskDir, const Core::ScanConfig &config)
{
    if (taskDir.trimmed().isEmpty() || !alignmentManager_) {
        return;
    }

    Core::AlignmentConfig alignment = alignmentEditor_ && alignmentEditor_->manager()
        ? alignmentEditor_->manager()->config()
        : alignmentManager_->config();
    alignment.enabled = true;
    if (!alignmentEditor_) {
        alignment.worldXMin = std::min(config.startX, config.endX);
        alignment.worldXMax = std::max(config.startX, config.endX);
        alignment.worldYMin = std::min(config.startY, config.endY);
        alignment.worldYMax = std::max(config.startY, config.endY);
        alignment.pixelXMin = 0.0;
        alignment.pixelXMax = 640.0;
        alignment.pixelYMin = 0.0;
        alignment.pixelYMax = 480.0;
    }
    alignmentManager_->setConfig(alignment);

    const QString path = QDir(taskDir).filePath(QStringLiteral("alignment.json"));
    if (alignmentManager_->saveToFile(path)) {
        appendLog(QStringLiteral("已写入 alignment.json：%1").arg(path));
    } else {
        appendLog(QStringLiteral("写入 alignment.json 失败：%1").arg(alignmentManager_->lastError()));
    }
}

QVector<MockScanPoint> ScanPage::buildMockScanPoints() const
{
    auto axisValues = [](double start, double end, double step) {
        QVector<double> values;
        const double absStep = std::max(std::abs(step), 0.0001);
        const double signedStep = end >= start ? absStep : -absStep;
        int guard = 0;
        constexpr int maxAxisPoints = 1000;

        if (signedStep > 0.0) {
            for (double value = start; value <= end + 0.000001 && guard < maxAxisPoints; value += signedStep, ++guard) {
                values.push_back(value);
            }
        } else {
            for (double value = start; value >= end - 0.000001 && guard < maxAxisPoints; value += signedStep, ++guard) {
                values.push_back(value);
            }
        }

        if (values.isEmpty()) {
            values.push_back(start);
        }
        return values;
    };

    const double startX = scanTableValue(0, 0.0);
    const double startY = scanTableValue(1, 0.0);
    const double startZ = scanTableValue(2, 0.0);
    const double endX = scanTableValue(3, 10.0);
    const double endY = scanTableValue(4, 10.0);
    const double endZ = scanTableValue(5, 1.0);
    const double stepX = scanTableValue(6, 0.5);
    const double stepY = scanTableValue(7, 0.5);
    const double stepZ = scanTableValue(8, 0.5);

    const QVector<double> xs = axisValues(startX, endX, stepX);
    const QVector<double> ys = axisValues(startY, endY, stepY);
    const QVector<double> zs = axisValues(startZ, endZ, stepZ);

    QVector<MockScanPoint> points;
    points.reserve(std::min<qsizetype>(xs.size() * ys.size() * zs.size(), 50000));
    for (double z : zs) {
        for (double y : ys) {
            for (double x : xs) {
                if (points.size() >= 50000) {
                    return points;
                }
                points.push_back(MockScanPoint{x, y, z});
            }
        }
    }
    return points;
}

void ScanPage::updateScanProgress(int current, int total)
{
    if (!scanProgressBar_) {
        return;
    }

    scanProgressBar_->setRange(0, std::max(1, total));
    scanProgressBar_->setValue(std::clamp(current, 0, std::max(1, total)));
    scanProgressBar_->setFormat(QStringLiteral("%1 / %2").arg(current).arg(total));
}

double ScanPage::scanTableValue(int column, double fallback) const
{
    if (!scanTable_ || column < 0 || column >= scanTable_->columnCount()) {
        return fallback;
    }

    const auto *item = scanTable_->item(0, column);
    if (!item) {
        return fallback;
    }

    bool ok = false;
    const double value = item->text().toDouble(&ok);
    return ok ? value : fallback;
}

void ScanPage::setScanTableValue(int column, double value)
{
    if (!scanTable_ || column < 0 || column >= scanTable_->columnCount()) {
        return;
    }

    auto *item = scanTable_->item(0, column);
    if (!item) {
        item = new QTableWidgetItem;
        scanTable_->setItem(0, column, item);
    }
    item->setText(mmText(value));
    item->setTextAlignment(Qt::AlignCenter);
}

} // namespace NFSScanner::UI
