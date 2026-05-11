#include "ui/MainWindow.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStringList>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QtGlobal>

#include <algorithm>
#include <utility>

#include "app/AppContext.h"
#include "core/ScanConfig.h"
#include "core/ScanWorker.h"
#include "devices/motion/IMotionController.h"
#include "devices/spectrum/ISpectrumAnalyzer.h"
#include "ui/HeatmapView.h"
#include "ui_MainWindow.h"

namespace NFSScanner::UI {

namespace {

QLabel *createValueLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setObjectName(QStringLiteral("valueLabel"));
    return label;
}

QDoubleSpinBox *createMmSpinBox(double value = 0.0)
{
    auto *spin = new QDoubleSpinBox;
    spin->setRange(-1000.0, 1000.0);
    spin->setDecimals(3);
    spin->setSingleStep(0.1);
    spin->setSuffix(QStringLiteral(" mm"));
    spin->setValue(value);
    return spin;
}

QDoubleSpinBox *createFrequencySpinBox(double value)
{
    auto *spin = new QDoubleSpinBox;
    spin->setRange(1.0, 110000.0);
    spin->setDecimals(3);
    spin->setSingleStep(10.0);
    spin->setSuffix(QStringLiteral(" MHz"));
    spin->setValue(value);
    return spin;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      appContext_(std::make_unique<App::AppContext>())
{
    ::Ui::MainWindow baseUi;
    baseUi.setupUi(this);

    setWindowTitle(QStringLiteral("NFS Scanner - C++/Qt6"));
    setObjectName(QStringLiteral("mainWindow"));

    setupMenus();
    setupCentralUi();
    setupConnections();

    setScanControlsEnabled(false);
    statusBar()->showMessage(QStringLiteral("就绪"));
    appendLog(QStringLiteral("系统初始化完成，当前使用 Mock 设备与 Mock 扫描流程。"));
}

MainWindow::~MainWindow() = default;

void MainWindow::setupMenus()
{
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("文件"));
    auto *quitAction = fileMenu->addAction(QStringLiteral("退出"));
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    auto *deviceMenu = menuBar()->addMenu(QStringLiteral("设备"));
    connectDevicesAction_ = deviceMenu->addAction(QStringLiteral("连接 Mock 设备"));
    disconnectDevicesAction_ = deviceMenu->addAction(QStringLiteral("断开设备"));

    auto *scanMenu = menuBar()->addMenu(QStringLiteral("扫描"));
    startScanAction_ = scanMenu->addAction(QStringLiteral("开始扫描"));
    stopScanAction_ = scanMenu->addAction(QStringLiteral("停止扫描"));

    auto *viewMenu = menuBar()->addMenu(QStringLiteral("视图"));
    auto *zoomInAction = viewMenu->addAction(QStringLiteral("放大"));
    auto *zoomOutAction = viewMenu->addAction(QStringLiteral("缩小"));
    auto *resetZoomAction = viewMenu->addAction(QStringLiteral("重置缩放"));

    connect(zoomInAction, &QAction::triggered, this, [this]() {
        heatmapView_->setZoomFactor(heatmapView_->zoomFactor() * 1.25);
    });
    connect(zoomOutAction, &QAction::triggered, this, [this]() {
        heatmapView_->setZoomFactor(heatmapView_->zoomFactor() * 0.8);
    });
    connect(resetZoomAction, &QAction::triggered, this, [this]() {
        heatmapView_->setZoomFactor(1.0);
    });

    auto *helpMenu = menuBar()->addMenu(QStringLiteral("帮助"));
    auto *aboutAction = helpMenu->addAction(QStringLiteral("关于"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this,
                           QStringLiteral("关于 NFS Scanner"),
                           QStringLiteral("NFS Scanner C++/Qt6 商业版重构骨架\n当前版本：0.1.0"));
    });
}

void MainWindow::setupCentralUi()
{
    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(10);

    auto *workspaceSplitter = new QSplitter(Qt::Horizontal, central);
    workspaceSplitter->setChildrenCollapsible(false);
    workspaceSplitter->addWidget(createLeftPanel());

    heatmapView_ = new HeatmapView(workspaceSplitter);
    workspaceSplitter->addWidget(heatmapView_);
    workspaceSplitter->addWidget(createRightPanel());
    workspaceSplitter->setStretchFactor(0, 0);
    workspaceSplitter->setStretchFactor(1, 1);
    workspaceSplitter->setStretchFactor(2, 0);
    workspaceSplitter->setSizes(QList<int>{300, 820, 320});

    auto *logGroup = new QGroupBox(QStringLiteral("运行日志"), central);
    auto *logLayout = new QVBoxLayout(logGroup);
    logLayout->setContentsMargins(10, 10, 10, 10);
    logEdit_ = new QPlainTextEdit(logGroup);
    logEdit_->setReadOnly(true);
    logEdit_->setMaximumBlockCount(1200);
    logEdit_->setMinimumHeight(130);
    logEdit_->setObjectName(QStringLiteral("logEdit"));
    logLayout->addWidget(logEdit_);

    rootLayout->addWidget(workspaceSplitter, 1);
    rootLayout->addWidget(logGroup, 0);

    setCentralWidget(central);
}

void MainWindow::setupConnections()
{
    auto *worker = appContext_->scanWorker();

    connect(startScanAction_, &QAction::triggered, this, &MainWindow::handleStartScan);
    connect(stopScanAction_, &QAction::triggered, this, &MainWindow::handleStopScan);
    connect(connectDevicesAction_, &QAction::triggered, this, &MainWindow::connectMockDevices);
    connect(disconnectDevicesAction_, &QAction::triggered, this, &MainWindow::disconnectMockDevices);

    connect(startScanButton_, &QPushButton::clicked, this, &MainWindow::handleStartScan);
    connect(stopScanButton_, &QPushButton::clicked, this, &MainWindow::handleStopScan);
    connect(connectDevicesButton_, &QPushButton::clicked, this, &MainWindow::connectMockDevices);
    connect(disconnectDevicesButton_, &QPushButton::clicked, this, &MainWindow::disconnectMockDevices);

    connect(worker, &Core::ScanWorker::progressChanged, this, &MainWindow::handleScanProgress);
    connect(worker, &Core::ScanWorker::logMessage, this, &MainWindow::appendLog);
    connect(worker, &Core::ScanWorker::scanFinished, this, &MainWindow::handleScanFinished);

    auto *motion = appContext_->motionController();
    connect(motion, &Devices::Motion::IMotionController::connectionChanged, this, [this](bool connected) {
        motionStatusLabel_->setText(connected ? QStringLiteral("已连接") : QStringLiteral("未连接"));
    });
    connect(motion, &Devices::Motion::IMotionController::positionChanged, this,
            [this](double x, double y, double z) {
                motionPositionLabel_->setText(QStringLiteral("X %1 / Y %2 / Z %3 mm")
                                                  .arg(x, 0, 'f', 2)
                                                  .arg(y, 0, 'f', 2)
                                                  .arg(z, 0, 'f', 2));
            });
    connect(motion, &Devices::Motion::IMotionController::errorOccurred, this, &MainWindow::appendLog);

    auto *spectrum = appContext_->spectrumAnalyzer();
    connect(spectrum, &Devices::Spectrum::ISpectrumAnalyzer::connectionChanged, this, [this](bool connected) {
        spectrumStatusLabel_->setText(connected ? QStringLiteral("已连接") : QStringLiteral("未连接"));
    });
    connect(spectrum, &Devices::Spectrum::ISpectrumAnalyzer::centerFrequencyChanged, this,
            [this](double mhz) {
                centerFrequencySpin_->setValue(mhz);
            });
    connect(spectrum, &Devices::Spectrum::ISpectrumAnalyzer::errorOccurred, this, &MainWindow::appendLog);
    connect(centerFrequencySpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), spectrum,
            &Devices::Spectrum::ISpectrumAnalyzer::setCenterFrequency);

    connect(moveButton_, &QPushButton::clicked, this, [this]() {
        const bool moved = appContext_->motionController()->moveTo(motionXSpin_->value(),
                                                                   motionYSpin_->value(),
                                                                   motionZSpin_->value());
        if (moved) {
            appendLog(QStringLiteral("运动平台移动到目标位置。"));
        }
    });
}

QWidget *MainWindow::createLeftPanel()
{
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *deviceGroup = new QGroupBox(QStringLiteral("设备连接"), content);
    auto *deviceLayout = new QGridLayout(deviceGroup);
    connectDevicesButton_ = new QPushButton(QStringLiteral("连接全部"), deviceGroup);
    disconnectDevicesButton_ = new QPushButton(QStringLiteral("断开"), deviceGroup);
    motionStatusLabel_ = createValueLabel(QStringLiteral("未连接"));
    spectrumStatusLabel_ = createValueLabel(QStringLiteral("未连接"));
    deviceLayout->addWidget(new QLabel(QStringLiteral("运动控制")), 0, 0);
    deviceLayout->addWidget(motionStatusLabel_, 0, 1);
    deviceLayout->addWidget(new QLabel(QStringLiteral("频谱仪")), 1, 0);
    deviceLayout->addWidget(spectrumStatusLabel_, 1, 1);
    deviceLayout->addWidget(connectDevicesButton_, 2, 0);
    deviceLayout->addWidget(disconnectDevicesButton_, 2, 1);

    auto *motionGroup = new QGroupBox(QStringLiteral("运动控制"), content);
    auto *motionLayout = new QFormLayout(motionGroup);
    motionXSpin_ = createMmSpinBox();
    motionYSpin_ = createMmSpinBox();
    motionZSpin_ = createMmSpinBox();
    motionPositionLabel_ = createValueLabel(QStringLiteral("X 0.00 / Y 0.00 / Z 0.00 mm"));
    moveButton_ = new QPushButton(QStringLiteral("移动到目标"), motionGroup);
    motionLayout->addRow(QStringLiteral("X"), motionXSpin_);
    motionLayout->addRow(QStringLiteral("Y"), motionYSpin_);
    motionLayout->addRow(QStringLiteral("Z"), motionZSpin_);
    motionLayout->addRow(QStringLiteral("当前位置"), motionPositionLabel_);
    motionLayout->addRow(moveButton_);

    auto *scanGroup = new QGroupBox(QStringLiteral("扫描参数"), content);
    auto *scanLayout = new QFormLayout(scanGroup);
    columnsSpin_ = new QSpinBox(scanGroup);
    columnsSpin_->setRange(1, 400);
    columnsSpin_->setValue(40);
    rowsSpin_ = new QSpinBox(scanGroup);
    rowsSpin_->setRange(1, 400);
    rowsSpin_->setValue(30);
    stepSpin_ = createMmSpinBox(1.0);
    stepSpin_->setRange(0.001, 100.0);
    startFrequencySpin_ = createFrequencySpinBox(1000.0);
    stopFrequencySpin_ = createFrequencySpinBox(3000.0);
    progressBar_ = new QProgressBar(scanGroup);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    startScanButton_ = new QPushButton(QStringLiteral("开始扫描"), scanGroup);
    startScanButton_->setObjectName(QStringLiteral("primaryButton"));
    stopScanButton_ = new QPushButton(QStringLiteral("停止扫描"), scanGroup);

    auto *scanButtons = new QWidget(scanGroup);
    auto *scanButtonsLayout = new QHBoxLayout(scanButtons);
    scanButtonsLayout->setContentsMargins(0, 0, 0, 0);
    scanButtonsLayout->addWidget(startScanButton_);
    scanButtonsLayout->addWidget(stopScanButton_);

    scanLayout->addRow(QStringLiteral("X 点数"), columnsSpin_);
    scanLayout->addRow(QStringLiteral("Y 点数"), rowsSpin_);
    scanLayout->addRow(QStringLiteral("步进"), stepSpin_);
    scanLayout->addRow(QStringLiteral("起始频率"), startFrequencySpin_);
    scanLayout->addRow(QStringLiteral("终止频率"), stopFrequencySpin_);
    scanLayout->addRow(QStringLiteral("进度"), progressBar_);
    scanLayout->addRow(scanButtons);

    layout->addWidget(deviceGroup);
    layout->addWidget(motionGroup);
    layout->addWidget(scanGroup);
    layout->addStretch(1);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(content);
    scroll->setMinimumWidth(280);
    scroll->setMaximumWidth(360);
    scroll->setFrameShape(QFrame::NoFrame);
    return scroll;
}

QWidget *MainWindow::createRightPanel()
{
    auto *content = new QWidget;
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *spectrumGroup = new QGroupBox(QStringLiteral("频谱仪设置"), content);
    auto *spectrumLayout = new QFormLayout(spectrumGroup);
    centerFrequencySpin_ = createFrequencySpinBox(1000.0);
    auto *rbwSpin = createFrequencySpinBox(1.0);
    rbwSpin->setRange(0.001, 1000.0);
    auto *vbwSpin = createFrequencySpinBox(1.0);
    vbwSpin->setRange(0.001, 1000.0);
    auto *detectorCombo = new QComboBox(spectrumGroup);
    detectorCombo->addItems(QStringList{QStringLiteral("Peak"), QStringLiteral("Average"), QStringLiteral("RMS")});
    spectrumLayout->addRow(QStringLiteral("中心频率"), centerFrequencySpin_);
    spectrumLayout->addRow(QStringLiteral("RBW"), rbwSpin);
    spectrumLayout->addRow(QStringLiteral("VBW"), vbwSpin);
    spectrumLayout->addRow(QStringLiteral("Detector"), detectorCombo);

    auto *traceGroup = new QGroupBox(QStringLiteral("Trace 选择"), content);
    auto *traceLayout = new QFormLayout(traceGroup);
    traceCombo_ = new QComboBox(traceGroup);
    traceCombo_->addItems(QStringList{QStringLiteral("Trace A"), QStringLiteral("Trace B"), QStringLiteral("Trace C")});
    auto *traceModeCombo = new QComboBox(traceGroup);
    traceModeCombo->addItems(QStringList{QStringLiteral("Clear/Write"), QStringLiteral("Max Hold"), QStringLiteral("Min Hold")});
    traceLayout->addRow(QStringLiteral("Trace"), traceCombo_);
    traceLayout->addRow(QStringLiteral("模式"), traceModeCombo);

    auto *frequencyGroup = new QGroupBox(QStringLiteral("频率选择"), content);
    auto *frequencyLayout = new QFormLayout(frequencyGroup);
    auto *spanSpin = createFrequencySpinBox(2000.0);
    auto *markerSpin = createFrequencySpinBox(1500.0);
    frequencyLayout->addRow(QStringLiteral("Span"), spanSpin);
    frequencyLayout->addRow(QStringLiteral("Marker"), markerSpin);

    auto *lutGroup = new QGroupBox(QStringLiteral("LUT 设置"), content);
    auto *lutLayout = new QFormLayout(lutGroup);
    auto *lutCombo = new QComboBox(lutGroup);
    lutCombo->addItems(QStringList{QStringLiteral("Viridis"),
                                   QStringLiteral("Thermal"),
                                   QStringLiteral("Turbo"),
                                   QStringLiteral("Gray")});
    auto *opacitySlider = new QSlider(Qt::Horizontal, lutGroup);
    opacitySlider->setRange(0, 100);
    opacitySlider->setValue(82);
    auto *overlayCheck = new QCheckBox(QStringLiteral("启用叠加"), lutGroup);
    overlayCheck->setChecked(true);
    lutLayout->addRow(QStringLiteral("色表"), lutCombo);
    lutLayout->addRow(QStringLiteral("透明度"), opacitySlider);
    lutLayout->addRow(overlayCheck);

    layout->addWidget(spectrumGroup);
    layout->addWidget(traceGroup);
    layout->addWidget(frequencyGroup);
    layout->addWidget(lutGroup);
    layout->addStretch(1);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(content);
    scroll->setMinimumWidth(300);
    scroll->setMaximumWidth(380);
    scroll->setFrameShape(QFrame::NoFrame);
    return scroll;
}

Core::ScanConfig MainWindow::buildScanConfig() const
{
    Core::ScanConfig config;
    config.columns = columnsSpin_->value();
    config.rows = rowsSpin_->value();
    config.stepMm = stepSpin_->value();
    config.startFrequencyMhz = startFrequencySpin_->value();
    config.stopFrequencyMhz = stopFrequencySpin_->value();

    if (config.stopFrequencyMhz < config.startFrequencyMhz) {
        std::swap(config.startFrequencyMhz, config.stopFrequencyMhz);
    }

    return config;
}

void MainWindow::handleStartScan()
{
    if (scanRunning_) {
        return;
    }

    const Core::ScanConfig config = buildScanConfig();
    appContext_->scanWorker()->setConfig(config);
    heatmapView_->clearHeatmapImage();
    heatmapView_->setScanProgress(0, config.totalPoints());
    progressBar_->setValue(0);

    scanRunning_ = true;
    setScanControlsEnabled(true);
    statusBar()->showMessage(QStringLiteral("扫描运行中"));
    appContext_->scanWorker()->startMockScan();
}

void MainWindow::handleStopScan()
{
    appContext_->scanWorker()->stopScan();
}

void MainWindow::handleScanProgress(int currentPoint, int totalPoints, const Core::ScanPoint &point)
{
    Q_UNUSED(point)

    const int percent = totalPoints > 0
        ? qRound(static_cast<double>(currentPoint) * 100.0 / static_cast<double>(totalPoints))
        : 0;
    progressBar_->setValue(percent);
    heatmapView_->setScanProgress(currentPoint, totalPoints);
}

void MainWindow::handleScanFinished(bool completed)
{
    scanRunning_ = false;
    setScanControlsEnabled(false);
    statusBar()->showMessage(completed ? QStringLiteral("扫描完成") : QStringLiteral("扫描已停止"));
}

void MainWindow::appendLog(const QString &message)
{
    if (!logEdit_) {
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    logEdit_->appendPlainText(QStringLiteral("[%1] %2").arg(timestamp, message));
}

void MainWindow::setScanControlsEnabled(bool running)
{
    startScanAction_->setEnabled(!running);
    startScanButton_->setEnabled(!running);
    stopScanAction_->setEnabled(running);
    stopScanButton_->setEnabled(running);

    columnsSpin_->setEnabled(!running);
    rowsSpin_->setEnabled(!running);
    stepSpin_->setEnabled(!running);
    startFrequencySpin_->setEnabled(!running);
    stopFrequencySpin_->setEnabled(!running);
}

void MainWindow::connectMockDevices()
{
    const bool motionOk = appContext_->motionController()->connectDevice();
    const bool spectrumOk = appContext_->spectrumAnalyzer()->connectDevice();

    appendLog(QStringLiteral("设备连接结果：运动控制=%1，频谱仪=%2。")
                  .arg(motionOk ? QStringLiteral("成功") : QStringLiteral("失败"),
                       spectrumOk ? QStringLiteral("成功") : QStringLiteral("失败")));
    statusBar()->showMessage(QStringLiteral("Mock 设备已连接"));
}

void MainWindow::disconnectMockDevices()
{
    appContext_->motionController()->disconnectDevice();
    appContext_->spectrumAnalyzer()->disconnectDevice();
    appendLog(QStringLiteral("Mock 设备已断开。"));
    statusBar()->showMessage(QStringLiteral("设备已断开"));
}

} // namespace NFSScanner::UI
