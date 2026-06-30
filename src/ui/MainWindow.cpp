#include "ui/MainWindow.h"

#include "app/AppVersion.h"
#include "analysis/FrequencyCsvParser.h"
#include "analysis/HeatmapGenerator.h"
#include "analysis/LutManager.h"
#include "core/ScanPathPlanner.h"
#include "core/AlignmentManager.h"
#include "core/DeviceManager.h"
#include "core/ScanManager.h"
#include "license/LicenseManager.h"
#include "project/ProjectManager.h"
#include "report/ReportData.h"
#include "report/ReportGenerator.h"
#include "devices/camera/ICamera.h"
#include "devices/motion/SerialMotionController.h"
#include "devices/spectrum/ISpectrumAnalyzer.h"
#include "devices/spectrum/SpectrumAnalyzerFactory.h"
#include "ui/AnalysisController.h"
#include "ui/AlignmentEditor.h"
#include "ui/DeviceStatusBar.h"
#include "ui/HeatmapDialog.h"
#include "ui/HeatmapView.h"
#include "ui/pages/AnalysisPage.h"
#include "ui/pages/DevicePage.h"
#include "ui/pages/ReportPage.h"
#include "ui/pages/ScanPage.h"

#include <QAbstractScrollArea>
#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QDoubleValidator>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIntValidator>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QPixmap>
#include <QScrollArea>
#include <QScrollBar>
#include <QSerialPortInfo>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QTime>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantMap>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <optional>

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

QLineEdit *createDoubleEdit(const QString &value, QWidget *parent)
{
    auto *edit = new QLineEdit(value, parent);
    auto *validator = new QDoubleValidator(-1000000.0, 1000000.0, 4, edit);
    validator->setNotation(QDoubleValidator::StandardNotation);
    edit->setValidator(validator);
    edit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return edit;
}

QLineEdit *createIntegerEdit(const QString &value, QWidget *parent)
{
    auto *edit = new QLineEdit(value, parent);
    edit->setValidator(new QIntValidator(1, 10000000, edit));
    edit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return edit;
}

QComboBox *createUnitCombo(const QStringList &units, QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    combo->addItems(units);
    return combo;
}

QWidget *createReservedInstrumentPage(const QString &text)
{
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(12, 12, 12, 12);
    auto *label = new QLabel(text, page);
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    layout->addStretch(1);
    layout->addWidget(label);
    layout->addStretch(1);
    return page;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setObjectName(QStringLiteral("mainWindow"));
    setWindowTitle(QStringLiteral(APP_NAME " v" APP_VERSION " - 近场扫描系统"));
    resize(1600, 900);

    deviceManager_ = new Core::DeviceManager(this);
    projectManager_ = new Project::ProjectManager(this);
    licenseManager_ = new License::LicenseManager(this);
    motionController_ = deviceManager_->motionController();

    analysisController_ = new AnalysisController(this);
    connect(analysisController_, &AnalysisController::previewUpdated, this, [this]() {
        applyHeatmapPreviewToCanvases();
        updateColorbarDisplay();
        if (analysisPage_ && analysisPage_->previewCanvas()) {
            HeatmapView::GridMapping mapping;
            mapping.xs = analysisController_->frequencyData().xs();
            mapping.ys = analysisController_->frequencyData().ys();
            const QVector<double> zs = analysisController_->frequencyData().zs();
            mapping.z = zs.isEmpty() ? 0.0 : zs.first();
            analysisPage_->previewCanvas()->setGridMapping(mapping);
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

    setupUi();
    setupStatusBar();

    connect(deviceManager_, &Core::DeviceManager::logMessage, this, &MainWindow::appendLog);
    connect(projectManager_, &Project::ProjectManager::logMessage, this, &MainWindow::appendLog);
    connect(projectManager_, &Project::ProjectManager::projectChanged, this, [this]() {
        updateProjectStatusDisplay();
        if (resultDirEdit_ && projectManager_->hasOpenProject()) {
            resultDirEdit_->setText(projectManager_->defaultScanOutputDir());
        }
    });

    scanManager_ = new Core::ScanManager(this);
    setupMotionController();
    setupScanManager();

    clockTimer_ = new QTimer(this);
    connect(clockTimer_, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    clockTimer_->start(1000);

    mockScanTimer_ = new QTimer(this);
    mockScanTimer_->setInterval(100);
    connect(mockScanTimer_, &QTimer::timeout, this, &MainWindow::advanceMockScan);

    if (deviceStatusBar_) {
        deviceStatusBar_->bindDeviceManager(deviceManager_);
    }

    if (analysisPage_ && analysisPage_->previewCanvas()) {
        analysisPage_->previewCanvas()->setCrosshairEnabled(true);
        connect(analysisPage_->previewCanvas(), &HeatmapView::cursorSampleChanged,
                this, &MainWindow::updateHeatmapCursorReadout);
    }

    updateActionButtons();
    updateStatusBar();
    appendLog(QStringLiteral("系统初始化完成，默认启用模拟模式，未发送真实串口命令。"));
}

MainWindow::~MainWindow()
{
    clearCurrentAnalyzer();
    if (motionController_ && motionController_->isOpen()) {
        motionController_->closePort();
    }
}

void MainWindow::setupUi()
{
    setupMenus();
    setupToolBar();
    setupNavigation();
    setupParamDock();
    setupAuxiliaryDocks();
    switchToPage(AppPage::Scan);
}

void MainWindow::setupMenus()
{
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    fileMenu->addAction(QStringLiteral("新建项目"), this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择项目父目录"),
                                                              projectManager_ ? projectManager_->workspaceRoot() : QString());
        if (dir.isEmpty()) {
            return;
        }
        bool ok = false;
        const QString name = QInputDialog::getText(this, QStringLiteral("新建项目"),
                                                   QStringLiteral("项目名称："), QLineEdit::Normal,
                                                   QStringLiteral("NewProject"), &ok);
        if (!ok || name.trimmed().isEmpty()) {
            return;
        }
        if (projectManager_->createProject(name.trimmed(), dir)) {
            appendLog(QStringLiteral("项目已创建：%1").arg(projectManager_->currentProject().rootPath));
        } else {
            QMessageBox::warning(this, QStringLiteral("新建项目失败"), projectManager_->lastError());
        }
    });
    fileMenu->addAction(QStringLiteral("打开项目"), this, [this]() {
        const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("打开项目文件夹"));
        if (path.isEmpty()) {
            return;
        }
        if (!projectManager_->openProject(path)) {
            QMessageBox::warning(this, QStringLiteral("打开项目失败"), projectManager_->lastError());
        }
    });
    fileMenu->addAction(QStringLiteral("保存项目"), this, [this]() {
        if (!projectManager_->saveProject()) {
            QMessageBox::warning(this, QStringLiteral("保存项目失败"), projectManager_->lastError());
        }
    });
    fileMenu->addAction(QStringLiteral("另存为"), this, [this]() {
        const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("另存为项目目录"));
        if (path.isEmpty()) {
            return;
        }
        if (!projectManager_->saveProjectAs(path)) {
            QMessageBox::warning(this, QStringLiteral("另存为失败"), projectManager_->lastError());
        }
    });
    auto *recentMenu = fileMenu->addMenu(QStringLiteral("最近项目"));
    if (projectManager_) {
        for (const QString &recent : projectManager_->recentProjects()) {
            recentMenu->addAction(recent, this, [this, recent]() {
                projectManager_->openProject(recent);
            });
        }
    }
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("退出"), this, &QWidget::close);

    menuBar()->addMenu(QStringLiteral("编辑(&E)"));

    auto *viewMenu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
    viewMenu->addAction(QStringLiteral("显示参数面板"), this, [this]() {
        if (paramDock_) {
            paramDock_->setVisible(!paramDock_->isVisible());
        }
    });
    viewMenu->addSeparator();
    viewMenu->addAction(QStringLiteral("日志"), this, [this]() {
        if (logDock_) {
            logDock_->setVisible(!logDock_->isVisible());
        }
    });
    viewMenu->addAction(QStringLiteral("频谱"), this, [this]() {
        if (spectrumDock_) {
            spectrumDock_->setVisible(!spectrumDock_->isVisible());
        }
    });
    viewMenu->addAction(QStringLiteral("统计"), this, [this]() {
        if (statisticsDock_) {
            statisticsDock_->setVisible(!statisticsDock_->isVisible());
        }
    });
    viewMenu->addAction(QStringLiteral("数据表格"), this, [this]() {
        if (dataTableDock_) {
            dataTableDock_->setVisible(!dataTableDock_->isVisible());
        }
    });

    auto *toolsMenu = menuBar()->addMenu(QStringLiteral("工具(&T)"));
    toolsMenu->addAction(QStringLiteral("导出图片"), this, [this]() { showHeatmap(); });
    toolsMenu->addAction(QStringLiteral("导出报告 (HTML)"), this, [this]() { exportCurrentReport(QStringLiteral("html")); });
    toolsMenu->addAction(QStringLiteral("导出分析配置 JSON"), this, &MainWindow::exportAnalysisConfigJson);
    toolsMenu->addAction(QStringLiteral("打开数据目录"), this, [this]() {
        const QString path = resultDirEdit_ ? resultDirEdit_->text().trimmed() : QString();
        if (!path.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    });

    auto *deviceMenu = menuBar()->addMenu(QStringLiteral("设备(&D)"));
    deviceMenu->addAction(QStringLiteral("刷新设备"), this, [this]() {
        if (deviceManager_) {
            deviceManager_->refreshDevices();
        }
        refreshSerialPorts();
    });
    deviceMenu->addAction(QStringLiteral("连接全部"), this, [this]() {
        if (deviceManager_) {
            deviceManager_->connectAll();
        }
    });
    deviceMenu->addAction(QStringLiteral("断开全部"), this, [this]() {
        if (deviceManager_) {
            deviceManager_->disconnectAll();
        }
    });

    auto *scanMenu = menuBar()->addMenu(QStringLiteral("扫描(&S)"));
    scanMenu->addAction(QStringLiteral("开始"), this, &MainWindow::startScan);
    scanMenu->addAction(QStringLiteral("暂停/继续"), this, &MainWindow::pauseScan);
    scanMenu->addAction(QStringLiteral("停止"), this, &MainWindow::stopScan);

    menuBar()->addMenu(QStringLiteral("设置(&S)"));

    auto *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    helpMenu->addAction(QStringLiteral("关于"), this, &MainWindow::showAboutDialog);
    helpMenu->addAction(QStringLiteral("诊断信息"), this, &MainWindow::showDiagnosticsDialog);
}

void MainWindow::setupToolBar()
{
    mainToolBar_ = addToolBar(QStringLiteral("mainToolBar"));
    mainToolBar_->setObjectName(QStringLiteral("mainToolBar"));
    mainToolBar_->setMovable(false);

    auto *startAction = mainToolBar_->addAction(QStringLiteral("开始扫描"), this, &MainWindow::startScan);
    startAction->setObjectName(QStringLiteral("toolbarStartScanAction"));
    auto *pauseAction = mainToolBar_->addAction(QStringLiteral("暂停"), this, &MainWindow::pauseScan);
    pauseAction->setObjectName(QStringLiteral("toolbarPauseScanAction"));
    auto *stopAction = mainToolBar_->addAction(QStringLiteral("停止"), this, &MainWindow::stopScan);
    stopAction->setObjectName(QStringLiteral("toolbarStopScanAction"));
    Q_UNUSED(pauseAction)
    Q_UNUSED(stopAction)
}

void MainWindow::setupNavigation()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralWidget"));
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(6, 6, 6, 6);
    rootLayout->setSpacing(6);

    navList_ = new QListWidget(central);
    navList_->setObjectName(QStringLiteral("leftNavigationBar"));
    navList_->setFixedWidth(72);
    navList_->setSpacing(2);
    navList_->setFocusPolicy(Qt::NoFocus);

    const QStringList navLabels{
        QStringLiteral("扫描"),
        QStringLiteral("设备"),
        QStringLiteral("分析"),
        QStringLiteral("报告"),
    };
    for (int i = 0; i < navLabels.size(); ++i) {
        auto *item = new QListWidgetItem(navLabels.at(i));
        item->setTextAlignment(Qt::AlignCenter);
        item->setData(Qt::UserRole, i);
        item->setSizeHint(QSize(64, 52));
        navList_->addItem(item);
    }
    navList_->setCurrentRow(0);

    setupPages();

    pageStack_ = new QStackedWidget(central);
    pageStack_->setObjectName(QStringLiteral("pageStack"));
    pageStack_->addWidget(scanPage_);
    pageStack_->addWidget(devicePage_);
    pageStack_->addWidget(analysisPage_);
    pageStack_->addWidget(reportPage_);

    rootLayout->addWidget(navList_);
    rootLayout->addWidget(pageStack_, 1);
    setCentralWidget(central);

    connect(navList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0 || row > 3) {
            return;
        }
        switchToPage(static_cast<AppPage>(row));
    });
}

void MainWindow::setupParamDock()
{
    paramDockStack_ = new QStackedWidget(this);
    paramDockStack_->setObjectName(QStringLiteral("paramDockStack"));

    auto *scanDockPage = new QWidget(paramDockStack_);
    auto *scanDockLayout = new QVBoxLayout(scanDockPage);
    scanDockLayout->setContentsMargins(4, 4, 4, 4);
    scanDockLayout->setSpacing(6);
    scanDockLayout->addWidget(createScanAreaGroup());
    scanDockLayout->addWidget(createTestInfoGroup());
    scanDockLayout->addWidget(createStepConfigGroup());
    scanDockLayout->addWidget(createActionGroup());
    alignmentEditor_ = new AlignmentEditor(scanDockPage);
    scanDockLayout->addWidget(alignmentEditor_);
    connect(alignmentEditor_, &AlignmentEditor::configApplied, this, [this](const Core::AlignmentConfig &config) {
        alignmentManager_.setConfig(config);
        applyAlignmentToHeatmapView(config);
    });
    scanDockLayout->addStretch(1);
    paramDockStack_->addWidget(scanDockPage);

    auto *deviceDockPage = new QWidget(paramDockStack_);
    auto *deviceDockLayout = new QVBoxLayout(deviceDockPage);
    deviceDockLayout->setContentsMargins(4, 4, 4, 4);
    deviceDockLayout->addWidget(createInstrumentGroup());
    deviceDockLayout->addStretch(1);
    paramDockStack_->addWidget(deviceDockPage);

    auto *analysisDockPage = new QWidget(paramDockStack_);
    auto *analysisDockLayout = new QVBoxLayout(analysisDockPage);
    analysisDockLayout->setContentsMargins(4, 4, 4, 4);
    analysisDockLayout->addWidget(createResultGroup());
    analysisDockLayout->addStretch(1);
    paramDockStack_->addWidget(analysisDockPage);

    auto *reportDockPage = new QWidget(paramDockStack_);
    auto *reportDockLayout = new QVBoxLayout(reportDockPage);
    reportDockLayout->setContentsMargins(8, 8, 8, 8);
    auto *exportHtmlButton = new QPushButton(QStringLiteral("导出 HTML 报告"), reportDockPage);
    auto *exportMdButton = new QPushButton(QStringLiteral("导出 Markdown 报告"), reportDockPage);
    auto *exportPdfButton = new QPushButton(QStringLiteral("导出 PDF 报告"), reportDockPage);
    auto *exportPngButton = new QPushButton(QStringLiteral("导出 PNG 图片集"), reportDockPage);
    reportDockLayout->addWidget(new QLabel(QStringLiteral("报告导出"), reportDockPage));
    reportDockLayout->addWidget(exportHtmlButton);
    reportDockLayout->addWidget(exportMdButton);
    reportDockLayout->addWidget(exportPdfButton);
    reportDockLayout->addWidget(exportPngButton);
    reportDockLayout->addStretch(1);
    connect(exportHtmlButton, &QPushButton::clicked, this, [this]() { exportCurrentReport(QStringLiteral("html")); });
    connect(exportMdButton, &QPushButton::clicked, this, [this]() { exportCurrentReport(QStringLiteral("md")); });
    connect(exportPdfButton, &QPushButton::clicked, this, [this]() { exportCurrentReport(QStringLiteral("pdf")); });
    connect(exportPngButton, &QPushButton::clicked, this, [this]() { exportCurrentReport(QStringLiteral("png")); });
    paramDockStack_->addWidget(reportDockPage);

    paramDock_ = new QDockWidget(QStringLiteral("扫描参数"), this);
    paramDock_->setObjectName(QStringLiteral("scanParamDock"));
    paramDock_->setWidget(paramDockStack_);
    paramDock_->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, paramDock_);
    paramDock_->setMinimumWidth(320);
}

void MainWindow::setupAuxiliaryDocks()
{
    logDock_ = new QDockWidget(QStringLiteral("日志"), this);
    logDock_->setObjectName(QStringLiteral("logDock"));
    logDock_->setWidget(createLogGroup());
    addDockWidget(Qt::BottomDockWidgetArea, logDock_);
    logDock_->hide();

    spectrumDock_ = new QDockWidget(QStringLiteral("频谱"), this);
    spectrumDock_->setObjectName(QStringLiteral("spectrumDock"));
    auto *spectrumPage = new QWidget(spectrumDock_);
    auto *spectrumLayout = new QVBoxLayout(spectrumPage);
    auto *spectrumHint = new QLabel(QStringLiteral("频谱面板：扫描完成后在此显示迹线曲线（待实现）。"), spectrumPage);
    spectrumHint->setWordWrap(true);
    spectrumHint->setAlignment(Qt::AlignCenter);
    spectrumLayout->addStretch(1);
    spectrumLayout->addWidget(spectrumHint);
    spectrumLayout->addStretch(1);
    spectrumDock_->setWidget(spectrumPage);
    addDockWidget(Qt::BottomDockWidgetArea, spectrumDock_);
    spectrumDock_->hide();

    statisticsDock_ = new QDockWidget(QStringLiteral("统计"), this);
    statisticsDock_->setObjectName(QStringLiteral("statisticsDock"));
    auto *statsPage = new QWidget(statisticsDock_);
    auto *statsLayout = new QVBoxLayout(statsPage);
    auto *statsHint = new QLabel(QStringLiteral("统计面板：幅度 min/max/mean 等（待实现）。"), statsPage);
    statsHint->setWordWrap(true);
    statsHint->setAlignment(Qt::AlignCenter);
    statsLayout->addStretch(1);
    statsLayout->addWidget(statsHint);
    statsLayout->addStretch(1);
    statisticsDock_->setWidget(statsPage);
    addDockWidget(Qt::BottomDockWidgetArea, statisticsDock_);
    statisticsDock_->hide();

    dataTableDock_ = new QDockWidget(QStringLiteral("数据表格"), this);
    dataTableDock_->setObjectName(QStringLiteral("dataTableDock"));
    auto *tablePage = new QWidget(dataTableDock_);
    auto *tableLayout = new QVBoxLayout(tablePage);
    auto *tableHint = new QLabel(QStringLiteral("数据表格：points.csv / traces.csv 预览（待实现）。"), tablePage);
    tableHint->setWordWrap(true);
    tableHint->setAlignment(Qt::AlignCenter);
    tableLayout->addStretch(1);
    tableLayout->addWidget(tableHint);
    tableLayout->addStretch(1);
    dataTableDock_->setWidget(tablePage);
    addDockWidget(Qt::BottomDockWidgetArea, dataTableDock_);
    dataTableDock_->hide();
}

void MainWindow::setupPages()
{
    scanPage_ = new ScanPage(this);
    heatmapView_ = scanPage_->canvas();

    devicePage_ = new DevicePage(this);
    if (auto *deviceLayout = qobject_cast<QVBoxLayout *>(devicePage_->contentHost()->layout())) {
        deviceLayout->insertWidget(0, createSerialGroup());
        deviceLayout->insertWidget(1, createMotionControlGroup());
        deviceLayout->insertWidget(2, createMotionCommandGroup());

        auto *diagnosticsGroup = new QGroupBox(QStringLiteral("系统诊断"), devicePage_->contentHost());
        auto *diagLayout = new QFormLayout(diagnosticsGroup);
        diagLayout->addRow(QStringLiteral("版本"), new QLabel(QStringLiteral(APP_NAME " v" APP_VERSION), diagnosticsGroup));
        diagLayout->addRow(QStringLiteral("Qt"), new QLabel(QStringLiteral(QT_VERSION_STR), diagnosticsGroup));
        diagLayout->addRow(QStringLiteral("授权"), new QLabel(licenseManager_
            ? License::licenseStatusText(licenseManager_->status())
            : QStringLiteral("Demo"), diagnosticsGroup));
        diagLayout->addRow(QStringLiteral("Machine ID"), new QLabel(licenseManager_
            ? licenseManager_->machineId()
            : QStringLiteral("-"), diagnosticsGroup));
        deviceLayout->insertWidget(3, diagnosticsGroup);

        auto *cameraPlaceholder = new QGroupBox(QStringLiteral("相机（可选）"), devicePage_->contentHost());
        auto *cameraLayout = new QVBoxLayout(cameraPlaceholder);
        auto *cameraConnectBtn = new QPushButton(QStringLiteral("连接 Mock Camera"), cameraPlaceholder);
        auto *cameraCaptureBtn = new QPushButton(QStringLiteral("拍照"), cameraPlaceholder);
        cameraLayout->addWidget(new QLabel(QStringLiteral("相机模块可选，不影响扫描数据采集。"), cameraPlaceholder));
        cameraLayout->addWidget(cameraConnectBtn);
        cameraLayout->addWidget(cameraCaptureBtn);
        deviceLayout->insertWidget(4, cameraPlaceholder);
        connect(cameraConnectBtn, &QPushButton::clicked, this, [this]() {
            if (deviceManager_) {
                deviceManager_->connectCamera();
            }
        });
        connect(cameraCaptureBtn, &QPushButton::clicked, this, [this]() {
            if (deviceManager_ && deviceManager_->camera()) {
                deviceManager_->camera()->captureFrame();
            }
        });

        auto *servoPlaceholder = new QGroupBox(QStringLiteral("舵机 / Hx·Hy（占位）"), devicePage_->contentHost());
        auto *servoLayout = new QVBoxLayout(servoPlaceholder);
        servoLayout->addWidget(new QLabel(QStringLiteral("Hx/Hy 探头切换将在 Release 014C 实现。"), servoPlaceholder));
        deviceLayout->insertWidget(5, servoPlaceholder);
    }

    analysisPage_ = new AnalysisPage(this);

    reportPage_ = new ReportPage(this);
}

void MainWindow::updateProjectStatusDisplay()
{
    if (!statusTextLabel_ || !projectManager_) {
        return;
    }
    updateStatusBar();
}

void MainWindow::exportCurrentReport(const QString &format)
{
    Report::ReportData data;
    data.projectName = projectManager_ && projectManager_->hasOpenProject()
        ? projectManager_->currentProject().name
        : (projectNameEdit_ ? projectNameEdit_->text() : QStringLiteral("Demo"));
    data.scanTaskDir = resultDirEdit_ ? resultDirEdit_->text() : QString();
    data.scanTime = QDateTime::currentDateTime();
    data.traceId = traceCombo_ ? traceCombo_->currentText() : QString();
    data.lutName = lutCombo_ ? lutCombo_->currentText() : QStringLiteral("turbo");
    data.vmin = analysisController_ ? analysisController_->vmin() : 0.0;
    data.vmax = analysisController_ ? analysisController_->vmax() : 1.0;
    data.heatmapImage = analysisController_ ? analysisController_->heatmapImage() : QImage();
    data.notes = testNameEdit_ ? testNameEdit_->text() : QString();
    if (currentSpectrumConfig_.stopFreqHz > 0.0) {
        data.startFrequencyHz = currentSpectrumConfig_.startFreqHz;
        data.stopFrequencyHz = currentSpectrumConfig_.stopFreqHz;
    }
    data.deviceSummary = deviceManager_
        ? QStringLiteral("运动:%1 频谱:%2 相机:%3")
              .arg(Core::deviceConnectionStateText(deviceManager_->motionState()),
                   Core::deviceConnectionStateText(deviceManager_->spectrumState()),
                   Core::deviceConnectionStateText(deviceManager_->cameraState()))
        : QStringLiteral("Demo");

    Report::ReportGenerator generator;
    if (format == QStringLiteral("png")) {
        const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择 PNG 导出目录"));
        if (dir.isEmpty()) {
            return;
        }
        if (!generator.exportPngImages(data, dir)) {
            QMessageBox::warning(this, QStringLiteral("导出失败"), generator.lastError());
            return;
        }
        appendLog(QStringLiteral("PNG 图片集已导出：%1").arg(dir));
        return;
    }

    const QString filter = format == QStringLiteral("pdf")
        ? QStringLiteral("PDF (*.pdf)")
        : (format == QStringLiteral("md") ? QStringLiteral("Markdown (*.md)") : QStringLiteral("HTML (*.html)"));
    const QString defaultName = format == QStringLiteral("pdf")
        ? QStringLiteral("report.pdf")
        : (format == QStringLiteral("md") ? QStringLiteral("report.md") : QStringLiteral("report.html"));
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出报告"), defaultName, filter);
    if (path.isEmpty()) {
        return;
    }

    bool ok = false;
    if (format == QStringLiteral("pdf")) {
        ok = generator.exportPdf(data, path);
    } else if (format == QStringLiteral("md")) {
        ok = generator.exportMarkdown(data, path);
    } else {
        ok = generator.exportHtml(data, path);
    }
    if (!ok) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), generator.lastError());
        return;
    }
    appendLog(QStringLiteral("报告已导出：%1").arg(path));
    if (reportPage_ && reportPage_->previewEditor()) {
        reportPage_->previewEditor()->setPlainText(
            QStringLiteral("已导出 %1\n项目：%2\nTrace：%3\n路径：%4")
                .arg(format.toUpper(), data.projectName, data.traceId, path));
    }
}

void MainWindow::exportAnalysisConfigJson()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("trace"), traceCombo_ ? traceCombo_->currentText() : QString());
    obj.insert(QStringLiteral("frequency_hz"), frequencyCombo_ ? frequencyCombo_->currentData().toDouble() : 0.0);
    obj.insert(QStringLiteral("display_mode"), selectedDisplayMode());
    obj.insert(QStringLiteral("lut"), lutCombo_ ? lutCombo_->currentText() : QStringLiteral("turbo"));
    obj.insert(QStringLiteral("vmin"), analysisController_ ? analysisController_->vmin() : 0.0);
    obj.insert(QStringLiteral("vmax"), analysisController_ ? analysisController_->vmax() : 1.0);
    obj.insert(QStringLiteral("opacity"), opacitySlider_ ? opacitySlider_->value() : 85);

    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出分析配置"),
                                                      QStringLiteral("analysis_config.json"),
                                                      QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), file.errorString());
        return;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    appendLog(QStringLiteral("分析配置已导出：%1").arg(path));
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(this, QStringLiteral("关于"),
                       QStringLiteral("%1 v%2\n\n近场扫描系统 Qt C++ 客户端。").arg(QStringLiteral(APP_NAME), QStringLiteral(APP_VERSION)));
}

void MainWindow::showDiagnosticsDialog()
{
    const QString text = QStringLiteral(
        "构建版本：%1 v%2\nQt：%3\nMachine ID：%4\n授权：%5\nWorkspace：%6\n数据格式：%7")
                             .arg(QStringLiteral(APP_NAME),
                                  QStringLiteral(APP_VERSION),
                                  QStringLiteral(QT_VERSION_STR),
                                  licenseManager_ ? licenseManager_->machineId() : QStringLiteral("-"),
                                  licenseManager_ ? License::licenseStatusText(licenseManager_->status()) : QStringLiteral("Demo"),
                                  projectManager_ ? projectManager_->workspaceRoot() : QStringLiteral("-"),
                                  QStringLiteral(DATA_FORMAT_VERSION));
    QMessageBox::information(this, QStringLiteral("诊断信息"), text);
}

void MainWindow::switchToPage(AppPage page)
{
    currentPage_ = page;
    if (pageStack_) {
        pageStack_->setCurrentIndex(static_cast<int>(page));
    }
    if (paramDockStack_) {
        paramDockStack_->setCurrentIndex(static_cast<int>(page));
    }

    static const QStringList dockTitles{
        QStringLiteral("扫描参数"),
        QStringLiteral("设备配置"),
        QStringLiteral("分析参数"),
        QStringLiteral("报告设置"),
    };
    if (paramDock_ && static_cast<int>(page) < dockTitles.size()) {
        paramDock_->setWindowTitle(dockTitles.at(static_cast<int>(page)));
    }

    if (navList_ && navList_->currentRow() != static_cast<int>(page)) {
        const QSignalBlocker blocker(navList_);
        navList_->setCurrentRow(static_cast<int>(page));
    }
}

QGroupBox *MainWindow::createSerialGroup()
{
    auto *group = new QGroupBox(QStringLiteral("串口设置"), this);
    auto *layout = new QGridLayout(group);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(6);

    serialPortCombo_ = new QComboBox(group);
    serialPortCombo_->addItems(QStringList{QStringLiteral("COM1"), QStringLiteral("COM2"), QStringLiteral("COM3")});
    serialPortCombo_->setMinimumContentsLength(8);
    serialPortCombo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    baudRateCombo_ = new QComboBox(group);
    baudRateCombo_->addItems(QStringList{QStringLiteral("9600"),
                                         QStringLiteral("19200"),
                                         QStringLiteral("38400"),
                                         QStringLiteral("57600"),
                                         QStringLiteral("115200"),
                                         QStringLiteral("230400")});
    baudRateCombo_->setCurrentText(QStringLiteral("115200"));

    openSerialButton_ = new QPushButton(QStringLiteral("打开串口"), group);
    closeSerialButton_ = new QPushButton(QStringLiteral("关闭串口"), group);
    refreshSerialButton_ = new QPushButton(QStringLiteral("刷新串口"), group);
    mockModeCheck_ = new QCheckBox(QStringLiteral("模拟模式"), group);
    mockModeCheck_->setChecked(true);
    closeSerialButton_->setEnabled(false);

    layout->addWidget(new QLabel(QStringLiteral("端口号"), group), 0, 0);
    layout->addWidget(serialPortCombo_, 0, 1, 1, 2);
    layout->addWidget(new QLabel(QStringLiteral("波特率"), group), 1, 0);
    layout->addWidget(baudRateCombo_, 1, 1, 1, 2);
    layout->addWidget(mockModeCheck_, 2, 0, 1, 3);
    layout->addWidget(openSerialButton_, 3, 0);
    layout->addWidget(closeSerialButton_, 3, 1);
    layout->addWidget(refreshSerialButton_, 3, 2);

    connect(refreshSerialButton_, &QPushButton::clicked, this, &MainWindow::refreshSerialPorts);
    connect(openSerialButton_, &QPushButton::clicked, this, &MainWindow::openSerialPort);
    connect(closeSerialButton_, &QPushButton::clicked, this, &MainWindow::closeSerialPort);
    connect(mockModeCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        if (deviceManager_) {
            deviceManager_->setMotionMockMode(checked);
        }
        if (checked) {
            if (motionController_ && motionController_->isOpen()) {
                motionController_->closePort();
            }
            updateSerialButtons(false);
            setAppState(QStringLiteral("模拟模式"));
            appendLog(QStringLiteral("已切换到模拟模式，运动命令不会发送到真实串口。"));
        } else {
            updateSerialButtons(motionController_ && motionController_->isOpen());
            setAppState(motionController_ && motionController_->isOpen()
                            ? QStringLiteral("串口已连接")
                            : QStringLiteral("真实串口模式"));
            appendLog(QStringLiteral("已切换到真实串口模式，请先刷新并打开串口。"));
        }
    });

    return group;
}

QGroupBox *MainWindow::createMotionControlGroup()
{
    auto *group = new QGroupBox(QStringLiteral("运动控制"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setSpacing(7);

    auto *stepRow = new QWidget(group);
    auto *stepLayout = new QHBoxLayout(stepRow);
    stepLayout->setContentsMargins(0, 0, 0, 0);
    stepLayout->setSpacing(5);
    stepLayout->addWidget(new QLabel(QStringLiteral("点动步距"), stepRow));

    auto *stepGroup = new QButtonGroup(group);
    stepGroup->setExclusive(true);
    const QList<double> steps{0.01, 0.1, 1.0, 5.0, 10.0, 20.0};
    for (double step : steps) {
        auto *button = new QPushButton(QString::number(step, 'g', 3), stepRow);
        button->setObjectName(QStringLiteral("stepButton"));
        button->setCheckable(true);
        button->setToolTip(QStringLiteral("%1 mm").arg(QString::number(step, 'g', 3)));
        stepGroup->addButton(button);
        stepLayout->addWidget(button);
        if (std::abs(step - 1.0) < 0.000001) {
            button->setChecked(true);
        }
        connect(button, &QPushButton::clicked, this, [this, step]() {
            jogStep_ = step;
        });
    }
    stepLayout->addWidget(new QLabel(QStringLiteral("mm"), stepRow));
    stepLayout->addStretch(1);
    layout->addWidget(stepRow);

    auto *jogGrid = new QGridLayout;
    jogGrid->setHorizontalSpacing(6);
    jogGrid->setVerticalSpacing(6);
    auto makeJogButton = [this, group](const QString &text, const QString &axis, double direction) {
        auto *button = new QPushButton(text, group);
        button->setMinimumWidth(70);
        connect(button, &QPushButton::clicked, this, [this, axis, direction]() {
            jogAxis(axis, direction);
        });
        return button;
    };

    jogGrid->addWidget(makeJogButton(QStringLiteral("X+"), QStringLiteral("X"), 1.0), 0, 0);
    jogGrid->addWidget(makeJogButton(QStringLiteral("Y+"), QStringLiteral("Y"), 1.0), 0, 1);
    jogGrid->addWidget(makeJogButton(QStringLiteral("Z+"), QStringLiteral("Z"), 1.0), 0, 2);
    jogGrid->addWidget(makeJogButton(QStringLiteral("X-"), QStringLiteral("X"), -1.0), 1, 0);
    jogGrid->addWidget(makeJogButton(QStringLiteral("Y-"), QStringLiteral("Y"), -1.0), 1, 1);
    jogGrid->addWidget(makeJogButton(QStringLiteral("Z-"), QStringLiteral("Z"), -1.0), 1, 2);
    layout->addLayout(jogGrid);

    return group;
}

QGroupBox *MainWindow::createMotionCommandGroup()
{
    auto *group = new QGroupBox(QStringLiteral("运动命令"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setSpacing(7);

    auto *row1 = new QWidget(group);
    auto *row1Layout = new QHBoxLayout(row1);
    row1Layout->setContentsMargins(0, 0, 0, 0);
    auto *resetButton = new QPushButton(QStringLiteral("复位"), row1);
    auto *queryButton = new QPushButton(QStringLiteral("位置查询"), row1);
    row1Layout->addWidget(resetButton);
    row1Layout->addWidget(queryButton);
    layout->addWidget(row1);

    auto *row2 = new QWidget(group);
    auto *row2Layout = new QHBoxLayout(row2);
    row2Layout->setContentsMargins(0, 0, 0, 0);
    auto *versionButton = new QPushButton(QStringLiteral("读取版本"), row2);
    auto *helpButton = new QPushButton(QStringLiteral("帮助命令"), row2);
    row2Layout->addWidget(versionButton);
    row2Layout->addWidget(helpButton);
    layout->addWidget(row2);

    auto *absoluteRow = new QWidget(group);
    auto *absoluteLayout = new QHBoxLayout(absoluteRow);
    absoluteLayout->setContentsMargins(0, 0, 0, 0);
    absoluteLayout->setSpacing(5);
    absoluteXEdit_ = createDoubleEdit(QStringLiteral("0.00"), absoluteRow);
    absoluteYEdit_ = createDoubleEdit(QStringLiteral("0.00"), absoluteRow);
    absoluteZEdit_ = createDoubleEdit(QStringLiteral("0.00"), absoluteRow);
    feedEdit_ = createIntegerEdit(QStringLiteral("1000"), absoluteRow);
    auto *executeButton = new QPushButton(QStringLiteral("执行"), absoluteRow);

    absoluteLayout->addWidget(new QLabel(QStringLiteral("绝对坐标"), absoluteRow));
    absoluteLayout->addWidget(new QLabel(QStringLiteral("X"), absoluteRow));
    absoluteLayout->addWidget(absoluteXEdit_);
    absoluteLayout->addWidget(new QLabel(QStringLiteral("Y"), absoluteRow));
    absoluteLayout->addWidget(absoluteYEdit_);
    absoluteLayout->addWidget(new QLabel(QStringLiteral("Z"), absoluteRow));
    absoluteLayout->addWidget(absoluteZEdit_);
    absoluteLayout->addWidget(new QLabel(QStringLiteral("Feed"), absoluteRow));
    absoluteLayout->addWidget(feedEdit_);
    absoluteLayout->addWidget(executeButton);
    layout->addWidget(absoluteRow);

    connect(resetButton, &QPushButton::clicked, this, &MainWindow::resetPosition);
    connect(queryButton, &QPushButton::clicked, this, &MainWindow::queryPosition);
    connect(versionButton, &QPushButton::clicked, this, &MainWindow::readVersion);
    connect(helpButton, &QPushButton::clicked, this, &MainWindow::readHelp);
    connect(executeButton, &QPushButton::clicked, this, &MainWindow::executeAbsoluteMove);

    return group;
}

QGroupBox *MainWindow::createStepConfigGroup()
{
    auto *group = new QGroupBox(QStringLiteral("步长设置"), this);
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

    connect(stepXEdit_, &QLineEdit::editingFinished, this, &MainWindow::syncStepInputsToTable);
    connect(stepYEdit_, &QLineEdit::editingFinished, this, &MainWindow::syncStepInputsToTable);
    connect(stepZEdit_, &QLineEdit::editingFinished, this, &MainWindow::syncStepInputsToTable);
    connect(setStartButton, &QPushButton::clicked, this, [this]() {
        setCurrentPositionAsScanPoint(true);
    });
    connect(setEndButton, &QPushButton::clicked, this, [this]() {
        setCurrentPositionAsScanPoint(false);
    });

    return group;
}

QGroupBox *MainWindow::createTestInfoGroup()
{
    auto *group = new QGroupBox(QStringLiteral("测试说明"), this);
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

QGroupBox *MainWindow::createActionGroup()
{
    auto *group = new QGroupBox(QStringLiteral("功能操作区"), this);
    auto *layout = new QGridLayout(group);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setHorizontalSpacing(7);
    layout->setVerticalSpacing(7);

    startScanButton_ = new QPushButton(QStringLiteral("开始"), group);
    startScanButton_->setObjectName(QStringLiteral("primaryButton"));
    pauseScanButton_ = new QPushButton(QStringLiteral("暂停"), group);
    stopScanButton_ = new QPushButton(QStringLiteral("停止"), group);
    auto *clearLogButton = new QPushButton(QStringLiteral("清除日志"), group);
    auto *searchInstrumentButton = new QPushButton(QStringLiteral("搜索仪表"), group);

    layout->addWidget(startScanButton_, 0, 0);
    layout->addWidget(pauseScanButton_, 0, 1);
    layout->addWidget(stopScanButton_, 0, 2);
    layout->addWidget(clearLogButton, 1, 0);
    layout->addWidget(searchInstrumentButton, 1, 1);
    auto *previewPathButton = new QPushButton(QStringLiteral("预览路径"), group);
    layout->addWidget(previewPathButton, 1, 2);

    connect(previewPathButton, &QPushButton::clicked, this, &MainWindow::previewScanPath);

    scanProgressBar_ = new QProgressBar(group);
    scanProgressBar_->setRange(0, 1);
    scanProgressBar_->setValue(0);
    scanProgressBar_->setTextVisible(true);
    layout->addWidget(scanProgressBar_, 2, 0, 1, 3);

    connect(startScanButton_, &QPushButton::clicked, this, &MainWindow::startScan);
    connect(pauseScanButton_, &QPushButton::clicked, this, &MainWindow::pauseScan);
    connect(stopScanButton_, &QPushButton::clicked, this, &MainWindow::stopScan);
    connect(clearLogButton, &QPushButton::clicked, this, [this]() {
        if (logEdit_) {
            logEdit_->clear();
        }
    });
    connect(searchInstrumentButton, &QPushButton::clicked, this, [this]() {
        if (deviceDiscoveryLabel_) {
            deviceDiscoveryLabel_->setText(QStringLiteral("发现 Mock 仪表"));
        }
        appendLog(QStringLiteral("设备发现完成：发现 Mock 仪表。"));
    });

    return group;
}

QGroupBox *MainWindow::createScanAreaGroup()
{
    auto *group = new QGroupBox(QStringLiteral("扫描区域"), this);
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

QGroupBox *MainWindow::createInstrumentGroup()
{
    auto *group = new QGroupBox(QStringLiteral("仪表区域"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setSpacing(8);

    auto *connectGrid = new QGridLayout;
    connectGrid->setHorizontalSpacing(6);
    connectGrid->setVerticalSpacing(6);

    analyzerTypeCombo_ = new QComboBox(group);
    analyzerTypeCombo_->addItems(Devices::Spectrum::SpectrumAnalyzerFactory::availableAnalyzers());
    analyzerHostEdit_ = new QLineEdit(QStringLiteral("192.168.0.10"), group);
    analyzerPortEdit_ = createIntegerEdit(QStringLiteral("5025"), group);
    analyzerConnectButton_ = new QPushButton(QStringLiteral("连接"), group);
    analyzerDisconnectButton_ = new QPushButton(QStringLiteral("断开"), group);
    queryIdnButton_ = new QPushButton(QStringLiteral("查询 IDN"), group);
    applyAnalyzerConfigButton_ = new QPushButton(QStringLiteral("应用仪表配置"), group);
    singleSweepButton_ = new QPushButton(QStringLiteral("单次扫描"), group);
    acquisitionTimeoutSpin_ = new QSpinBox(group);
    acquisitionTimeoutSpin_->setRange(1000, 60000);
    acquisitionTimeoutSpin_->setValue(10000);
    acquisitionTimeoutSpin_->setSuffix(QStringLiteral(" ms"));
    acquisitionRetrySpin_ = new QSpinBox(group);
    acquisitionRetrySpin_->setRange(0, 5);
    acquisitionRetrySpin_->setValue(1);
    stopOnErrorCheck_ = new QCheckBox(QStringLiteral("失败停止"), group);
    stopOnErrorCheck_->setChecked(true);
    analyzerMethodLabel_ = new QLabel(group);
    analyzerMethodLabel_->setWordWrap(true);

    connectGrid->addWidget(new QLabel(QStringLiteral("频谱仪"), group), 0, 0);
    connectGrid->addWidget(analyzerTypeCombo_, 0, 1);
    connectGrid->addWidget(new QLabel(QStringLiteral("Host"), group), 0, 2);
    connectGrid->addWidget(analyzerHostEdit_, 0, 3);
    connectGrid->addWidget(new QLabel(QStringLiteral("Port"), group), 0, 4);
    connectGrid->addWidget(analyzerPortEdit_, 0, 5);
    connectGrid->addWidget(analyzerConnectButton_, 0, 6);
    connectGrid->addWidget(analyzerDisconnectButton_, 0, 7);
    connectGrid->addWidget(queryIdnButton_, 1, 0, 1, 2);
    connectGrid->addWidget(applyAnalyzerConfigButton_, 1, 2, 1, 3);
    connectGrid->addWidget(singleSweepButton_, 1, 5, 1, 3);
    connectGrid->addWidget(new QLabel(QStringLiteral("超时"), group), 2, 0);
    connectGrid->addWidget(acquisitionTimeoutSpin_, 2, 1);
    connectGrid->addWidget(new QLabel(QStringLiteral("重试次数"), group), 2, 2);
    connectGrid->addWidget(acquisitionRetrySpin_, 2, 3);
    connectGrid->addWidget(stopOnErrorCheck_, 2, 4, 1, 2);
    connectGrid->addWidget(new QLabel(QStringLiteral("采集方式"), group), 3, 0);
    connectGrid->addWidget(analyzerMethodLabel_, 3, 1, 1, 7);
    connectGrid->setColumnStretch(1, 1);
    connectGrid->setColumnStretch(3, 1);
    layout->addLayout(connectGrid);

    auto *tabs = new QTabWidget(group);
    tabs->setDocumentMode(false);

    auto *znaPage = new QWidget(tabs);
    auto *znaLayout = new QVBoxLayout(znaPage);
    znaLayout->setContentsMargins(8, 8, 8, 8);
    znaLayout->setSpacing(8);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(5, 1);

    auto addParameter = [this, grid, znaPage](int row,
                                              int columnOffset,
                                              const QString &labelText,
                                              const QString &value,
                                              const QStringList &units,
                                              const QString &fixedUnit = QString()) {
        auto *label = new QLabel(labelText, znaPage);
        auto *edit = createDoubleEdit(value, znaPage);
        QWidget *unitWidget = nullptr;
        if (fixedUnit.isEmpty()) {
            unitWidget = createUnitCombo(units, znaPage);
        } else {
            unitWidget = new QLabel(fixedUnit, znaPage);
        }
        auto *queryButton = new QPushButton(QStringLiteral("查询"), znaPage);

        grid->addWidget(label, row, columnOffset);
        grid->addWidget(edit, row, columnOffset + 1);
        grid->addWidget(unitWidget, row, columnOffset + 2);
        grid->addWidget(queryButton, row, columnOffset + 3);

        if (labelText == QStringLiteral("起始频率")) {
            startFreqEdit_ = edit;
            startFreqUnitCombo_ = qobject_cast<QComboBox *>(unitWidget);
        } else if (labelText == QStringLiteral("终止频率")) {
            stopFreqEdit_ = edit;
            stopFreqUnitCombo_ = qobject_cast<QComboBox *>(unitWidget);
        } else if (labelText == QStringLiteral("RBW")) {
            rbwEdit_ = edit;
            rbwUnitCombo_ = qobject_cast<QComboBox *>(unitWidget);
        }

        connect(queryButton, &QPushButton::clicked, this, [this, labelText, edit, unitWidget]() {
            QString unit;
            if (auto *combo = qobject_cast<QComboBox *>(unitWidget)) {
                unit = combo->currentText();
            } else if (auto *label = qobject_cast<QLabel *>(unitWidget)) {
                unit = label->text();
            }
            appendLog(QStringLiteral("查询%1：%2 %3").arg(labelText, edit->text(), unit));
        });
    };

    const QStringList freqUnits{QStringLiteral("MHz"), QStringLiteral("GHz"), QStringLiteral("Hz")};
    const QStringList rbwUnits{QStringLiteral("kHz"), QStringLiteral("MHz"), QStringLiteral("Hz")};
    addParameter(0, 0, QStringLiteral("起始频率"), QStringLiteral("1.00"), freqUnits);
    addParameter(1, 0, QStringLiteral("终止频率"), QStringLiteral("1000.00"), freqUnits);
    addParameter(2, 0, QStringLiteral("RBW"), QStringLiteral("100.00"), rbwUnits);
    addParameter(3, 0, QStringLiteral("Scale"), QStringLiteral("10.00"), QStringList{}, QStringLiteral("dB/div"));

    addParameter(0, 4, QStringLiteral("中心频率"), QStringLiteral("500.00"), freqUnits);
    addParameter(1, 4, QStringLiteral("Span"), QStringLiteral("999.00"), freqUnits);

    auto *pointsLabel = new QLabel(QStringLiteral("扫描点数"), znaPage);
    auto *pointsEdit = createIntegerEdit(QStringLiteral("201"), znaPage);
    sweepPointsEdit_ = pointsEdit;
    auto *pointsQueryButton = new QPushButton(QStringLiteral("查询"), znaPage);
    grid->addWidget(pointsLabel, 2, 4);
    grid->addWidget(pointsEdit, 2, 5, 1, 2);
    grid->addWidget(pointsQueryButton, 2, 7);
    connect(pointsQueryButton, &QPushButton::clicked, this, [this, pointsEdit]() {
        appendLog(QStringLiteral("查询扫描点数：%1").arg(pointsEdit->text()));
    });

    auto *presetButton = new QPushButton(QStringLiteral("Preset"), znaPage);
    auto *saveDataButton = new QPushButton(QStringLiteral("保存数据"), znaPage);
    auto *dataDirButton = new QPushButton(QStringLiteral("数据存储Dir"), znaPage);
    grid->addWidget(presetButton, 3, 4);
    grid->addWidget(saveDataButton, 3, 5);
    grid->addWidget(dataDirButton, 3, 6, 1, 2);
    connect(presetButton, &QPushButton::clicked, this, [this]() {
        appendLog(QStringLiteral("ZNA67 Preset 已执行（Mock）。"));
    });
    connect(saveDataButton, &QPushButton::clicked, this, [this]() {
        appendLog(QStringLiteral("ZNA67 保存数据命令已触发（Mock）。"));
    });
    connect(dataDirButton, &QPushButton::clicked, this, [this]() {
        appendLog(QStringLiteral("ZNA67 数据存储目录选择待接入。"));
    });

    znaLayout->addLayout(grid);

    auto *discoveryRow = new QWidget(znaPage);
    auto *discoveryLayout = new QHBoxLayout(discoveryRow);
    discoveryLayout->setContentsMargins(0, 0, 0, 0);
    discoveryLayout->addWidget(new QLabel(QStringLiteral("设备发现"), discoveryRow));
    deviceDiscoveryLabel_ = new QLabel(QStringLiteral("未匹配到 ZNA67"), discoveryRow);
    deviceDiscoveryLabel_->setObjectName(QStringLiteral("deviceDiscoveryLabel"));
    discoveryLayout->addWidget(deviceDiscoveryLabel_);
    discoveryLayout->addStretch(1);
    znaLayout->addWidget(discoveryRow);

    tabs->addTab(znaPage, QStringLiteral("ZNA67"));
    tabs->addTab(createReservedInstrumentPage(QStringLiteral("当前阶段为预留页，后续接入设备适配器。")),
                 QStringLiteral("N9020A"));
    tabs->addTab(createReservedInstrumentPage(QStringLiteral("当前阶段为预留页，后续接入设备适配器。")),
                 QStringLiteral("FSW"));
    tabs->setCurrentIndex(0);

    layout->addWidget(tabs);
    connect(analyzerConnectButton_, &QPushButton::clicked, this, &MainWindow::connectSpectrumAnalyzer);
    connect(analyzerDisconnectButton_, &QPushButton::clicked, this, &MainWindow::disconnectSpectrumAnalyzer);
    connect(queryIdnButton_, &QPushButton::clicked, this, &MainWindow::querySpectrumIdn);
    connect(applyAnalyzerConfigButton_, &QPushButton::clicked, this, &MainWindow::applySpectrumConfig);
    connect(singleSweepButton_, &QPushButton::clicked, this, &MainWindow::runSingleSpectrumSweep);
    connect(analyzerTypeCombo_, &QComboBox::currentTextChanged, this, &MainWindow::updateAnalyzerMethodHint);
    updateAnalyzerMethodHint(analyzerTypeCombo_->currentText());
    updateAnalyzerButtons(false);
    return group;
}

QGroupBox *MainWindow::createResultGroup()
{
    auto *group = new QGroupBox(QStringLiteral("结果区域"), this);
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
        const QString path = resultDirEdit_ ? resultDirEdit_->text().trimmed() : QString();
        const QFileInfo info(path);
        if (path.isEmpty() || !info.exists() || !info.isDir()) {
            appendLog(QStringLiteral("结果目录不存在：%1").arg(path.isEmpty() ? QStringLiteral("(空)") : path));
            return;
        }

        appendLog(QStringLiteral("打开结果目录：%1").arg(info.absoluteFilePath()));
        QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath()));
    });
    connect(loadDataButton, &QPushButton::clicked, this, &MainWindow::loadFrequencyData);
    connect(heatmapButton, &QPushButton::clicked, this, &MainWindow::showHeatmap);
    connect(autoRangeCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        if (vminSpin_) {
            vminSpin_->setEnabled(!checked);
        }
        if (vmaxSpin_) {
            vmaxSpin_->setEnabled(!checked);
        }
        scheduleHeatmapPreviewRefresh();
    });
    connect(opacitySlider_, &QSlider::valueChanged, this, &MainWindow::updateOpacityLabel);
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
            this, &MainWindow::scheduleHeatmapPreviewRefresh);
    connect(frequencyCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::scheduleHeatmapPreviewRefresh);
    connect(displayModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::scheduleHeatmapPreviewRefresh);
    connect(vminSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::scheduleHeatmapPreviewRefresh);
    connect(vmaxSpin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::scheduleHeatmapPreviewRefresh);

    updateColorbarDisplay();

    return group;
}

QGroupBox *MainWindow::createHeatmapPreviewGroup()
{
    auto *group = new QGroupBox(QStringLiteral("热力图预览"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 12, 10, 10);

    heatmapView_ = new HeatmapView(group);
    heatmapView_->setOpacityPercent(opacitySlider_ ? opacitySlider_->value() : 85);
    layout->addWidget(heatmapView_, 1);

    return group;
}

QGroupBox *MainWindow::createLogGroup()
{
    auto *group = new QGroupBox(QStringLiteral("日志区域"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 12, 10, 10);

    logEdit_ = new QPlainTextEdit(group);
    logEdit_->setReadOnly(true);
    logEdit_->setMaximumBlockCount(4000);
    logEdit_->setMinimumHeight(240);
    logEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(logEdit_);

    return group;
}

void MainWindow::setupStatusBar()
{
    statusBar_ = new QStatusBar(this);
    deviceStatusBar_ = new DeviceStatusBar(statusBar_);
    statusBar_->addWidget(deviceStatusBar_, 1);
    statusTextLabel_ = new QLabel(statusBar_);
    statusTextLabel_->setObjectName(QStringLiteral("statusTextLabel"));
    statusTextLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statusBar_->addPermanentWidget(statusTextLabel_, 2);
    setStatusBar(statusBar_);
}

void MainWindow::setupMotionController()
{
    if (!motionController_) {
        return;
    }

    connect(motionController_, &Devices::Motion::SerialMotionController::connectedChanged, this, [this](bool connected) {
        updateSerialButtons(connected);
        setAppState(connected ? QStringLiteral("串口已连接") : QStringLiteral("串口已关闭"));
    });
    connect(motionController_, &Devices::Motion::IMotionController::positionChanged, this, [this](double x, double y, double z) {
        currentX_ = x;
        currentY_ = y;
        currentZ_ = z;
        updateStatusBar();
    });
    connect(motionController_, &Devices::Motion::SerialMotionController::statusChanged, this, &MainWindow::setAppState);
    connect(motionController_, &Devices::Motion::SerialMotionController::logMessage, this, &MainWindow::appendLog);
    connect(motionController_, &Devices::Motion::SerialMotionController::rawLineReceived, this, [this](const QString &line) {
        appendLog(QStringLiteral("接收：%1").arg(line));
    });
    connect(motionController_, &Devices::Motion::IMotionController::errorOccurred, this, [this](const QString &message) {
        appendLog(QStringLiteral("错误：%1").arg(message));
    });
}

void MainWindow::setupScanManager()
{
    if (!scanManager_) {
        return;
    }

    scanManager_->setSpectrumDeviceHost(deviceManager_ ? deviceManager_->spectrumDeviceHost() : nullptr);
    scanManager_->setSpectrumDeviceThread(deviceManager_ ? deviceManager_->spectrumDeviceThread() : nullptr);

    connect(scanManager_, &Core::ScanManager::stateChanged, this, [this](const QString &stateText) {
        setAppState(stateText);
        updateActionButtons();
    });
    connect(scanManager_, &Core::ScanManager::logMessage, this, &MainWindow::appendLog);
    connect(scanManager_, &Core::ScanManager::progressChanged, this, [this](int current, int total) {
        updateScanProgress(current, total);
        if (heatmapView_ && analysisController_->heatmapImage().isNull()) {
            heatmapView_->setScanProgress(current, total);
        }
    });
    connect(scanManager_, &Core::ScanManager::currentPointChanged, this, [this](int, int, double x, double y, double z) {
        currentX_ = x;
        currentY_ = y;
        currentZ_ = z;
        updateStatusBar();
    });
    connect(scanManager_, &Core::ScanManager::estimatedChanged, this, [this](int remainingCount, int estimatedSeconds) {
        remainingText_ = QString::number(remainingCount);
        estimatedFinishText_ = QStringLiteral("%1s").arg(estimatedSeconds);
        updateStatusBar();
    });
    connect(scanManager_, &Core::ScanManager::taskDirChanged, this, [this](const QString &taskDir) {
        if (resultDirEdit_) {
            resultDirEdit_->setText(taskDir);
        }
        saveAlignmentForTaskDir(taskDir, readScanConfigFromUi());
    });
    connect(scanManager_, &Core::ScanManager::scanFinished, this, [this](const QString &taskDir) {
        remainingText_ = QStringLiteral("0");
        estimatedFinishText_ = QStringLiteral("--");
        if (resultDirEdit_) {
            resultDirEdit_->setText(taskDir);
        }
        appendLog(QStringLiteral("扫描完成，数据已保存：%1").arg(taskDir));
        loadFrequencyData();
        appendLog(QStringLiteral("扫描完成，可点击“显示热力图”。"));
        updateActionButtons();
        updateStatusBar();
    });
    connect(scanManager_, &Core::ScanManager::scanError, this, [this](const QString &message) {
        remainingText_ = QStringLiteral("--");
        estimatedFinishText_ = QStringLiteral("--");
        appendLog(QStringLiteral("扫描错误：%1").arg(message));
        updateActionButtons();
        updateStatusBar();
        QMessageBox::warning(this, QStringLiteral("扫描错误"), message);
    });
}

void MainWindow::appendLog(const QString &text)
{
    if (!logEdit_) {
        return;
    }

    logEdit_->appendPlainText(QStringLiteral("[%1] %2")
                                  .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), text));
    logEdit_->verticalScrollBar()->setValue(logEdit_->verticalScrollBar()->maximum());
}

void MainWindow::updateStatusBar()
{
    if (!statusTextLabel_) {
        return;
    }

    statusTextLabel_->setText(QStringLiteral("X: %1 mm | Y: %2 mm | Z: %3 mm | 时间: %4 | 剩余: %5 | 预计完成: %6 | 状态: %7")
                                  .arg(mmText(currentX_),
                                       mmText(currentY_),
                                       mmText(currentZ_),
                                       QTime::currentTime().toString(QStringLiteral("HH:mm:ss")),
                                       remainingText_,
                                       estimatedFinishText_,
                                       appState_));
}

void MainWindow::setAppState(const QString &state)
{
    appState_ = state;
    updateStatusBar();
}

bool MainWindow::isMockMode() const
{
    return !mockModeCheck_ || mockModeCheck_->isChecked();
}

double MainWindow::feedValue() const
{
    if (!feedEdit_ || feedEdit_->text().trimmed().isEmpty()) {
        return 1000.0;
    }

    bool ok = false;
    const double feed = feedEdit_->text().toDouble(&ok);
    return ok && feed > 0.0 ? feed : 1000.0;
}

bool MainWindow::ensureRealMotionReady()
{
    if (isMockMode()) {
        return false;
    }

    if (!motionController_ || !motionController_->isOpen()) {
        appendLog(QStringLiteral("请先打开串口。"));
        setAppState(QStringLiteral("串口未连接"));
        return false;
    }

    return true;
}

bool MainWindow::validateMotionTarget(double x, double y, double z)
{
    auto fail = [this](const QString &axis, double value, const QString &range) {
        appendLog(QStringLiteral("坐标越界：%1=%2，允许范围 %3")
                      .arg(axis, mmText(value), range));
        return false;
    };

    if (x < 0.0 || x > 200.0) {
        return fail(QStringLiteral("X"), x, QStringLiteral("0~200"));
    }
    if (y < -300.0 || y > 0.0) {
        return fail(QStringLiteral("Y"), y, QStringLiteral("-300~0"));
    }
    if (z < 0.0 || z > 10.0) {
        return fail(QStringLiteral("Z"), z, QStringLiteral("0~10"));
    }
    return true;
}

void MainWindow::updateSerialButtons(bool connected)
{
    if (openSerialButton_) {
        openSerialButton_->setEnabled(!connected);
    }
    if (closeSerialButton_) {
        closeSerialButton_->setEnabled(connected);
    }
}

void MainWindow::updateScanProgress(int current, int total)
{
    if (!scanProgressBar_) {
        return;
    }

    scanProgressBar_->setRange(0, std::max(1, total));
    scanProgressBar_->setValue(std::clamp(current, 0, std::max(1, total)));
    scanProgressBar_->setFormat(QStringLiteral("%1 / %2").arg(current).arg(total));
}

void MainWindow::loadFrequencyData()
{
    const QString tracePath = resolveTraceCsvPath();
    if (tracePath.isEmpty()) {
        const QString message = QStringLiteral("traces.csv 不存在，请选择任务目录或 CSV 文件。");
        appendLog(message);
        QMessageBox::warning(this, QStringLiteral("加载失败"), message);
        return;
    }

    QString errorMessage;
    if (!analysisController_ || !analysisController_->loadTraceCsv(tracePath, &errorMessage)) {
        appendLog(QStringLiteral("加载数据失败：%1").arg(errorMessage));
        QMessageBox::warning(this, QStringLiteral("加载失败"), errorMessage);
        return;
    }

    populateFrequencyControls();
    const auto &data = analysisController_->frequencyData();
    appendLog(QStringLiteral("已加载数据：trace数量=%1，频率点=%2，坐标点=%3")
                  .arg(data.traceIds().size())
                  .arg(data.frequencyCount())
                  .arg(data.pointCount()));
    scheduleHeatmapPreviewRefresh();
}

void MainWindow::populateFrequencyControls()
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

AnalysisRenderParams MainWindow::buildAnalysisParams() const
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

void MainWindow::showHeatmap()
{
    if (!analysisController_ || !analysisController_->frequencyData().isValid()) {
        const QString message = QStringLiteral("请先加载 traces.csv 数据。");
        appendLog(message);
        QMessageBox::warning(this, QStringLiteral("无法显示热力图"), message);
        return;
    }

    const QString traceId = traceCombo_ ? traceCombo_->currentText() : QString();
    if (traceId.isEmpty()) {
        const QString message = QStringLiteral("未选择 Trace。");
        appendLog(message);
        QMessageBox::warning(this, QStringLiteral("无法显示热力图"), message);
        return;
    }

    if (!refreshHeatmapPreview()) {
        const QString message = QStringLiteral("生成热力图失败，请检查 Trace/频率与数据范围。");
        appendLog(message);
        QMessageBox::warning(this, QStringLiteral("无法显示热力图"), message);
        return;
    }

    const QString title = QStringLiteral("%1 | %2 | %3")
                              .arg(traceId,
                                   frequencyCombo_ ? frequencyCombo_->currentText() : QStringLiteral("Frequency"),
                                   displayModeCombo_ ? displayModeCombo_->currentText() : QStringLiteral("幅度"));
    appendLog(QStringLiteral("热力图已生成：LUT=%1，范围=%2 ~ %3，透明度=%4%")
                  .arg(lutCombo_ ? lutCombo_->currentText() : QStringLiteral("turbo"),
                       QString::number(analysisController_->vmin(), 'g', 6),
                       QString::number(analysisController_->vmax(), 'g', 6),
                       QString::number(opacitySlider_ ? opacitySlider_->value() : 85)));

    HeatmapDialog dialog(analysisController_->heatmapImage(),
                         analysisController_->colorbarImage(),
                         title,
                         analysisController_->vmin(),
                         analysisController_->vmax(),
                         this);
    dialog.exec();
}

void MainWindow::scheduleHeatmapPreviewRefresh()
{
    if (!analysisController_ || !analysisController_->frequencyData().isValid()) {
        return;
    }
    analysisController_->scheduleRefresh(buildAnalysisParams());
}

bool MainWindow::refreshHeatmapPreview()
{
    if (!analysisController_) {
        return false;
    }
    return analysisController_->refreshPreview(buildAnalysisParams(), nullptr);
}

void MainWindow::applyHeatmapPreviewToCanvases()
{
    if (!analysisController_) {
        return;
    }
    const int opacity = opacitySlider_ ? opacitySlider_->value() : 85;
    const QImage heatmap = analysisController_->heatmapImage();
    if (heatmapView_) {
        heatmapView_->setOpacityPercent(opacity);
        heatmapView_->setHeatmapImage(heatmap);
    }
    if (analysisPage_ && analysisPage_->previewCanvas()) {
        analysisPage_->previewCanvas()->setOpacityPercent(opacity);
        analysisPage_->previewCanvas()->setHeatmapImage(heatmap);
    }
}

void MainWindow::updateHeatmapCursorReadout(double worldX, double worldY, bool insideImage)
{
    if (!analysisPage_ || !analysisPage_->hintLabel()) {
        return;
    }

    if (!insideImage || !analysisController_ || !analysisController_->frequencyData().isValid()) {
        analysisPage_->hintLabel()->setText(
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

    analysisPage_->hintLabel()->setText(
        QStringLiteral("光标：X=%1 mm  Y=%2 mm  Z=%3 mm  |  %4 = %5")
            .arg(QString::number(worldX, 'f', 2),
                 QString::number(worldY, 'f', 2),
                 QString::number(z, 'f', 2),
                 displayModeCombo_ ? displayModeCombo_->currentText() : QStringLiteral("值"),
                 valueText));
}

void MainWindow::updateColorbarDisplay()
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

void MainWindow::updateOpacityLabel(int percent)
{
    const int safePercent = std::clamp(percent, 0, 100);
    if (opacityLabel_) {
        opacityLabel_->setText(QStringLiteral("透明度 %1%").arg(safePercent));
    }
    if (heatmapView_) {
        heatmapView_->setOpacityPercent(safePercent);
    }
    if (analysisPage_ && analysisPage_->previewCanvas()) {
        analysisPage_->previewCanvas()->setOpacityPercent(safePercent);
    }
}

void MainWindow::clearCurrentAnalyzer()
{
    if (scanManager_) {
        scanManager_->setSpectrumAnalyzer(nullptr);
    }
    if (deviceManager_) {
        deviceManager_->disconnectSpectrumAnalyzer();
        deviceManager_->releaseSpectrumAnalyzer();
    }
    currentAnalyzer_ = nullptr;
    updateAnalyzerButtons(false);
}

void MainWindow::connectSpectrumAnalyzer()
{
    const QString analyzerName = analyzerTypeCombo_ ? analyzerTypeCombo_->currentText() : QStringLiteral("Mock Spectrum");
    clearCurrentAnalyzer();
    if (!deviceManager_ || !deviceManager_->createSpectrumAnalyzer(analyzerName)) {
        appendLog(QStringLiteral("频谱仪创建失败：%1").arg(deviceManager_ ? deviceManager_->lastError() : QString()));
        return;
    }

    bool portOk = false;
    const int port = analyzerPortEdit_ ? analyzerPortEdit_->text().toInt(&portOk) : 5025;
    QVariantMap options;
    options.insert(QStringLiteral("host"), analyzerHostEdit_ ? analyzerHostEdit_->text().trimmed() : QString());
    options.insert(QStringLiteral("port"), portOk ? port : 5025);

    if (!deviceManager_->connectSpectrumAnalyzer(options)) {
        const QString message = deviceManager_->lastError().isEmpty()
            ? QStringLiteral("频谱仪连接失败。")
            : deviceManager_->lastError();
        appendLog(QStringLiteral("频谱仪连接失败：%1").arg(message));
        QMessageBox::warning(this, QStringLiteral("频谱仪连接失败"), message);
        updateAnalyzerButtons(false);
        return;
    }

    currentAnalyzer_ = deviceManager_->spectrumAnalyzer();
    if (scanManager_) {
        scanManager_->setSpectrumAnalyzer(currentAnalyzer_);
    }
    currentSpectrumConfig_ = readSpectrumConfig();
    appendLog(QStringLiteral("频谱仪已连接（设备线程）：%1").arg(currentAnalyzer_ ? currentAnalyzer_->name() : analyzerName));
    updateAnalyzerButtons(true);
    if (deviceStatusBar_) {
        deviceStatusBar_->refresh();
    }
}

void MainWindow::disconnectSpectrumAnalyzer()
{
    if (deviceManager_) {
        deviceManager_->disconnectSpectrumAnalyzer();
    }
    if (currentAnalyzer_) {
        appendLog(QStringLiteral("频谱仪已断开：%1").arg(currentAnalyzer_->name()));
    } else {
        appendLog(QStringLiteral("当前没有已创建的频谱仪连接。"));
    }
    currentAnalyzer_ = nullptr;
    if (scanManager_) {
        scanManager_->setSpectrumAnalyzer(nullptr);
    }
    updateAnalyzerButtons(false);
    if (deviceStatusBar_) {
        deviceStatusBar_->refresh();
    }
}

void MainWindow::querySpectrumIdn()
{
    if (!deviceManager_ || !currentAnalyzer_ || !currentAnalyzer_->isConnected()) {
        const QString message = QStringLiteral("请先连接频谱仪。");
        appendLog(message);
        QMessageBox::warning(this, QStringLiteral("频谱仪未连接"), message);
        return;
    }

    const QString idn = deviceManager_->querySpectrumIdn();
    if (idn.isEmpty()) {
        const QString message = deviceManager_->lastError().isEmpty()
            ? QStringLiteral("IDN 查询无返回。")
            : deviceManager_->lastError();
        appendLog(QStringLiteral("IDN 查询失败：%1").arg(message));
        QMessageBox::warning(this, QStringLiteral("IDN 查询失败"), message);
        return;
    }

    appendLog(QStringLiteral("频谱仪 IDN：%1").arg(idn));
}

void MainWindow::applySpectrumConfig()
{
    if (!deviceManager_ || !currentAnalyzer_ || !currentAnalyzer_->isConnected()) {
        const QString message = QStringLiteral("请先连接仪表。");
        appendLog(message);
        QMessageBox::warning(this, QStringLiteral("频谱仪未连接"), message);
        return;
    }

    currentSpectrumConfig_ = readSpectrumConfig();
    if (!deviceManager_->configureSpectrum(currentSpectrumConfig_)) {
        const QString message = deviceManager_->lastError().isEmpty()
            ? QStringLiteral("应用仪表配置失败。")
            : deviceManager_->lastError();
        appendLog(QStringLiteral("应用仪表配置失败：%1").arg(message));
        QMessageBox::warning(this, QStringLiteral("配置失败"), message);
        return;
    }

    appendLog(QStringLiteral("应用仪表配置成功：%1 Hz ~ %2 Hz，RBW=%3 Hz，点数=%4")
                  .arg(currentSpectrumConfig_.startFreqHz, 0, 'f', 0)
                  .arg(currentSpectrumConfig_.stopFreqHz, 0, 'f', 0)
                  .arg(currentSpectrumConfig_.rbwHz, 0, 'f', 0)
                  .arg(currentSpectrumConfig_.sweepPoints));
}

void MainWindow::runSingleSpectrumSweep()
{
    if (!currentAnalyzer_ || !currentAnalyzer_->isConnected()) {
        const QString message = QStringLiteral("请先连接仪表。");
        appendLog(message);
        QMessageBox::warning(this, QStringLiteral("频谱仪未连接"), message);
        return;
    }

    appendLog(QStringLiteral("正在采集单次频谱，请稍候。"));
    lastSpectrumTrace_ = currentAnalyzer_->singleSweep(0, currentX_, currentY_, currentZ_);
    if (lastSpectrumTrace_.freqs.isEmpty()
        || lastSpectrumTrace_.values.isEmpty()
        || lastSpectrumTrace_.freqs.size() != lastSpectrumTrace_.values.size()) {
        const QString message = currentAnalyzer_->lastError().isEmpty()
            ? QStringLiteral("单次扫描未返回有效数据。")
            : currentAnalyzer_->lastError();
        appendLog(QStringLiteral("单次扫描失败：%1").arg(message));
        QMessageBox::warning(this, QStringLiteral("单次扫描失败"), message);
        return;
    }

    const auto [minIt, maxIt] = std::minmax_element(lastSpectrumTrace_.values.cbegin(), lastSpectrumTrace_.values.cend());
    appendLog(QStringLiteral("单次扫描完成：source=%1，trace=%2，点数=%3，范围=%4 ~ %5")
                  .arg(lastSpectrumTrace_.source.isEmpty() ? currentAnalyzer_->name() : lastSpectrumTrace_.source,
                       lastSpectrumTrace_.traceId)
                  .arg(lastSpectrumTrace_.values.size())
                  .arg(*minIt, 0, 'g', 6)
                  .arg(*maxIt, 0, 'g', 6));
    if (!lastSpectrumTrace_.components.isEmpty()) {
        QStringList traceNames;
        for (const auto &component : lastSpectrumTrace_.components) {
            traceNames << component.traceId;
        }
        appendLog(QStringLiteral("单次扫描 components=%1：%2")
                      .arg(lastSpectrumTrace_.components.size())
                      .arg(traceNames.join(QStringLiteral(", "))));
    } else {
        appendLog(QStringLiteral("单次扫描 components=0，使用主 trace values。"));
    }
}

void MainWindow::updateAnalyzerButtons(bool connected)
{
    if (analyzerConnectButton_) {
        analyzerConnectButton_->setEnabled(!connected);
    }
    if (analyzerDisconnectButton_) {
        analyzerDisconnectButton_->setEnabled(connected);
    }
    if (queryIdnButton_) {
        queryIdnButton_->setEnabled(connected);
    }
    if (applyAnalyzerConfigButton_) {
        applyAnalyzerConfigButton_->setEnabled(connected);
    }
    if (singleSweepButton_) {
        singleSweepButton_->setEnabled(connected);
    }
}

void MainWindow::updateAnalyzerMethodHint(const QString &analyzerName)
{
    if (!analyzerMethodLabel_) {
        return;
    }

    QString method = QStringLiteral("Mock");
    QString detail = QStringLiteral("Mock spectrum data, no hardware required.");
    if (analyzerName.contains(QStringLiteral("ZNA67"), Qt::CaseInsensitive)) {
        method = QStringLiteral("MMEM CSV");
        detail = QStringLiteral("Uses MMEM:STOR:TRAC:CHAN 1 and parses exported multi-trace re/im CSV.");
    } else if (analyzerName.contains(QStringLiteral("FSW"), Qt::CaseInsensitive)) {
        method = QStringLiteral("MMEM CSV");
        detail = QStringLiteral("Uses MMEM:STOR1:TRAC and parses frequency/amplitude CSV.");
    } else if (analyzerName.contains(QStringLiteral("N9020A"), Qt::CaseInsensitive)) {
        method = QStringLiteral("ASCII TRACE");
        detail = QStringLiteral("Uses FORM ASC + ABOR + INIT:IMM + TRAC:DATA? TRACE1.");
    } else if (analyzerName.contains(QStringLiteral("Generic"), Qt::CaseInsensitive)) {
        method = QStringLiteral("Generic SCPI");
        detail = QStringLiteral("Uses TRAC:DATA? TRACE1 or CALC:DATA? SDATA fallback.");
    }

    analyzerMethodLabel_->setText(QStringLiteral("%1 - %2").arg(method, detail));
}

NFSScanner::Devices::Spectrum::SpectrumConfig MainWindow::readSpectrumConfig() const
{
    NFSScanner::Devices::Spectrum::SpectrumConfig config;
    config.startFreqHz = readFrequencyWithUnit(startFreqEdit_, startFreqUnitCombo_);
    config.stopFreqHz = readFrequencyWithUnit(stopFreqEdit_, stopFreqUnitCombo_);
    if (config.stopFreqHz <= config.startFreqHz) {
        config.stopFreqHz = config.startFreqHz + 1.0;
    }
    config.centerFreqHz = (config.startFreqHz + config.stopFreqHz) * 0.5;
    config.spanHz = config.stopFreqHz - config.startFreqHz;
    config.rbwHz = readFrequencyWithUnit(rbwEdit_, rbwUnitCombo_);
    config.vbwHz = config.rbwHz;

    bool pointsOk = false;
    const int points = sweepPointsEdit_ ? sweepPointsEdit_->text().toInt(&pointsOk) : 201;
    config.sweepPoints = pointsOk ? std::clamp(points, 2, 1000000) : 201;
    config.sweepTimeSec = std::max(0.05, static_cast<double>(dwellTimeSpinBox_ ? dwellTimeSpinBox_->value() : 100) / 1000.0);
    config.traceId = QStringLiteral("Trc1_S21");
    return config;
}

double MainWindow::readFrequencyWithUnit(QLineEdit *edit, QComboBox *unitCombo) const
{
    bool ok = false;
    const double value = edit ? edit->text().trimmed().toDouble(&ok) : 0.0;
    const QString unit = unitCombo ? unitCombo->currentText().trimmed() : QStringLiteral("Hz");
    double factor = 1.0;
    if (unit == QStringLiteral("GHz")) {
        factor = 1e9;
    } else if (unit == QStringLiteral("MHz")) {
        factor = 1e6;
    } else if (unit == QStringLiteral("kHz")) {
        factor = 1e3;
    }
    return ok ? value * factor : 0.0;
}

QString MainWindow::selectedDisplayMode() const
{
    if (!displayModeCombo_) {
        return QStringLiteral("magnitude");
    }

    const QString mode = displayModeCombo_->currentData().toString();
    return mode.isEmpty() ? QStringLiteral("magnitude") : mode;
}

QString MainWindow::formatFrequency(double hz) const
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

QString MainWindow::resolveTraceCsvPath() const
{
    const QString path = resultDirEdit_ ? resultDirEdit_->text().trimmed() : QString();
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

void MainWindow::refreshSerialPorts()
{
    serialPortCombo_->clear();
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : ports) {
        serialPortCombo_->addItem(port.portName());
    }

    if (ports.isEmpty()) {
        appendLog(QStringLiteral("未发现可用串口。"));
    } else {
        appendLog(QStringLiteral("串口列表已刷新：发现 %1 个可用串口。").arg(ports.size()));
    }
}

void MainWindow::openSerialPort()
{
    if (isMockMode()) {
        updateSerialButtons(true);
        setAppState(QStringLiteral("串口已连接"));
        appendLog(QStringLiteral("当前为模拟模式，未发送真实串口命令。"));
        appendLog(QStringLiteral("模拟串口已连接：%1，%2")
                      .arg(serialPortCombo_->currentText().isEmpty() ? QStringLiteral("MOCK") : serialPortCombo_->currentText(),
                           baudRateCombo_->currentText()));
        return;
    }

    if (serialPortCombo_->count() == 0 || serialPortCombo_->currentText().trimmed().isEmpty()) {
        appendLog(QStringLiteral("请先刷新并选择可用串口。"));
        return;
    }

    bool baudOk = false;
    const int baudRate = baudRateCombo_->currentText().toInt(&baudOk);
    if (!baudOk) {
        appendLog(QStringLiteral("波特率无效：%1").arg(baudRateCombo_->currentText()));
        return;
    }

    if (motionController_) {
        motionController_->openPort(serialPortCombo_->currentText(), baudRate);
    }
}

void MainWindow::closeSerialPort()
{
    if (isMockMode()) {
        updateSerialButtons(false);
        setAppState(QStringLiteral("串口已关闭"));
        appendLog(QStringLiteral("模拟串口已关闭。"));
        return;
    }

    if (motionController_) {
        motionController_->closePort();
    }
}

void MainWindow::jogAxis(const QString &axis, double direction)
{
    if (!isMockMode()) {
        if (!ensureRealMotionReady()) {
            return;
        }
        motionController_->jogAxis(axis, direction * jogStep_, feedValue());
        return;
    }

    const double targetX = axis == QStringLiteral("X") ? currentX_ + direction * jogStep_ : currentX_;
    const double targetY = axis == QStringLiteral("Y") ? currentY_ + direction * jogStep_ : currentY_;
    const double targetZ = axis == QStringLiteral("Z") ? currentZ_ + direction * jogStep_ : currentZ_;
    if (!validateMotionTarget(targetX, targetY, targetZ)) {
        return;
    }

    if (axis == QStringLiteral("X")) {
        currentX_ = targetX;
    } else if (axis == QStringLiteral("Y")) {
        currentY_ = targetY;
    } else if (axis == QStringLiteral("Z")) {
        currentZ_ = targetZ;
    }

    updateStatusBar();
    appendLog(QStringLiteral("模拟模式：点动 %1%2 %3 mm，当前位置 %4")
                  .arg(axis,
                       direction > 0.0 ? QStringLiteral("+") : QStringLiteral("-"),
                       mmText(jogStep_),
                       positionText(currentX_, currentY_, currentZ_)));
}

void MainWindow::resetPosition()
{
    if (!isMockMode()) {
        if (ensureRealMotionReady()) {
            motionController_->home();
        }
        return;
    }

    currentX_ = 0.0;
    currentY_ = 0.0;
    currentZ_ = 0.0;
    updateStatusBar();
    appendLog(QStringLiteral("模拟模式：执行复位，当前位置已清零。"));
}

void MainWindow::queryPosition()
{
    if (!isMockMode()) {
        if (ensureRealMotionReady()) {
            motionController_->queryPosition();
        }
        return;
    }

    appendLog(QStringLiteral("模拟模式：当前位置 %1").arg(positionText(currentX_, currentY_, currentZ_)));
}

void MainWindow::readVersion()
{
    if (!isMockMode()) {
        if (ensureRealMotionReady()) {
            motionController_->readVersion();
        }
        return;
    }

    appendLog(QStringLiteral("模拟模式：Mock GRBL Controller v1.0"));
}

void MainWindow::readHelp()
{
    if (!isMockMode()) {
        if (ensureRealMotionReady()) {
            motionController_->readHelp();
        }
        return;
    }

    appendLog(QStringLiteral("模拟模式：支持命令 $H、?、$I、G1X..Y..Z..F.."));
}

void MainWindow::executeAbsoluteMove()
{
    auto readOptionalAxis = [this](QLineEdit *edit, const QString &axis, bool &ok) -> std::optional<double> {
        const QString text = edit ? edit->text().trimmed() : QString();
        if (text.isEmpty()) {
            return std::nullopt;
        }

        bool valueOk = false;
        const double value = text.toDouble(&valueOk);
        if (!valueOk) {
            appendLog(QStringLiteral("%1 坐标参数无效：%2").arg(axis, text));
            ok = false;
            return std::nullopt;
        }
        return value;
    };

    bool axesOk = true;
    const std::optional<double> x = readOptionalAxis(absoluteXEdit_, QStringLiteral("X"), axesOk);
    const std::optional<double> y = readOptionalAxis(absoluteYEdit_, QStringLiteral("Y"), axesOk);
    const std::optional<double> z = readOptionalAxis(absoluteZEdit_, QStringLiteral("Z"), axesOk);
    const double feed = feedValue();

    if (!axesOk) {
        appendLog(QStringLiteral("绝对坐标参数无效，G1 命令未执行。"));
        return;
    }

    if (!isMockMode()) {
        if (ensureRealMotionReady()) {
            motionController_->moveAbs(x, y, z, feed);
        }
        return;
    }

    const double targetX = x.value_or(currentX_);
    const double targetY = y.value_or(currentY_);
    const double targetZ = z.value_or(currentZ_);
    if (!validateMotionTarget(targetX, targetY, targetZ)) {
        return;
    }

    currentX_ = targetX;
    currentY_ = targetY;
    currentZ_ = targetZ;
    updateStatusBar();
    appendLog(QStringLiteral("模拟模式：执行 G1 X%1 Y%2 Z%3 F%4，当前位置 %5")
                  .arg(mmText(currentX_),
                       mmText(currentY_),
                       mmText(currentZ_),
                       QString::number(feed, 'f', 0),
                       positionText(currentX_, currentY_, currentZ_)));
}

void MainWindow::setCurrentPositionAsScanPoint(bool startPoint)
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

void MainWindow::syncStepInputsToTable()
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

void MainWindow::startScan()
{
    if (!scanManager_) {
        return;
    }

    if (currentPage_ != AppPage::Scan) {
        appendLog(QStringLiteral("请切换到扫描页后开始扫描。"));
        switchToPage(AppPage::Scan);
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
        QMessageBox::warning(this, QStringLiteral("无法开始扫描"), message);
        return;
    }
    applyPathPreviewToCanvas(config, previewPoints);
    appendLog(QStringLiteral("扫描路径：共 %1 个点（蛇形：%2）")
                  .arg(previewPoints.size())
                  .arg(config.snakeMode ? QStringLiteral("是") : QStringLiteral("否")));

    const bool useRealMotion = !isMockMode();
    if (useRealMotion && (!motionController_ || !motionController_->isOpen())) {
        const QString message = QStringLiteral("请先打开运动控制串口，或勾选模拟模式。");
        appendLog(message);
        QMessageBox::warning(this, QStringLiteral("运动控制未连接"), message);
        return;
    }

    remainingText_ = QStringLiteral("--");
    estimatedFinishText_ = QStringLiteral("--");
    updateScanProgress(0, 1);
    currentSpectrumConfig_ = readSpectrumConfig();
    scanManager_->setSpectrumConfig(currentSpectrumConfig_);
    scanManager_->setSpectrumAnalyzer(currentAnalyzer_ && currentAnalyzer_->isConnected() ? currentAnalyzer_ : nullptr);
    scanManager_->setMotionController(motionController_);
    scanManager_->setUseRealMotion(useRealMotion);
    Core::ScanAcquisitionOptions acquisitionOptions;
    acquisitionOptions.timeoutMs = acquisitionTimeoutSpin_ ? acquisitionTimeoutSpin_->value() : 10000;
    acquisitionOptions.retryCount = acquisitionRetrySpin_ ? acquisitionRetrySpin_->value() : 1;
    acquisitionOptions.stopOnError = !stopOnErrorCheck_ || stopOnErrorCheck_->isChecked();
    scanManager_->setAcquisitionOptions(acquisitionOptions);
    if (analyzerTypeCombo_
        && analyzerTypeCombo_->currentText() != QStringLiteral("Mock Spectrum")
        && (!currentAnalyzer_ || !currentAnalyzer_->isConnected())) {
        appendLog(QStringLiteral("真实仪表未连接，将使用 Mock Spectrum 数据完成扫描。"));
    }
    scanManager_->startScan(config);
    updateActionButtons();
}

void MainWindow::pauseScan()
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

void MainWindow::stopScan()
{
    if (!scanManager_) {
        return;
    }

    scanManager_->stopScan();
    remainingText_ = QStringLiteral("--");
    estimatedFinishText_ = QStringLiteral("--");
    updateActionButtons();
    updateStatusBar();
}

void MainWindow::advanceMockScan()
{
    if (scanIndex_ >= mockScanPoints_.size()) {
        finishMockScan();
        return;
    }

    const ScanPoint point = mockScanPoints_.at(scanIndex_);
    ++scanIndex_;
    currentX_ = point.x;
    currentY_ = point.y;
    currentZ_ = point.z;

    const int total = mockScanPoints_.size();
    const int remaining = std::max(0, total - scanIndex_);
    remainingText_ = QString::number(remaining);
    estimatedFinishText_ = QStringLiteral("%1 秒").arg(std::ceil(remaining / 10.0), 0, 'f', 0);
    setAppState(QStringLiteral("扫描中"));
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

void MainWindow::finishMockScan()
{
    if (mockScanTimer_->isActive()) {
        mockScanTimer_->stop();
    }
    remainingText_ = QStringLiteral("0");
    estimatedFinishText_ = QStringLiteral("--");
    setAppState(QStringLiteral("已完成"));
    appendLog(QStringLiteral("扫描完成。"));
    updateActionButtons();
}

void MainWindow::updateActionButtons()
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

Core::ScanConfig MainWindow::readScanConfigFromUi() const
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
    config.feed = feedValue();
    config.dwellMs = dwellTimeSpinBox_ ? dwellTimeSpinBox_->value() : 100;
    config.snakeMode = !snakeModeCheck_ || snakeModeCheck_->isChecked();
    config.projectName = projectNameEdit_ ? projectNameEdit_->text().trimmed() : QString();
    config.testName = testNameEdit_ ? testNameEdit_->text().trimmed() : QString();
    config.outputDir = projectManager_ && projectManager_->hasOpenProject()
        ? projectManager_->defaultScanOutputDir()
        : (resultDirEdit_ && !resultDirEdit_->text().trimmed().isEmpty()
               ? resultDirEdit_->text().trimmed()
               : QStringLiteral("data/scans"));
    return config;
}

void MainWindow::setScanParamsLocked(bool locked)
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

void MainWindow::previewScanPath()
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
        QMessageBox::warning(this, QStringLiteral("路径预览"), message);
        return;
    }
    applyPathPreviewToCanvas(config, points);
    appendLog(QStringLiteral("路径预览：%1 个点。").arg(points.size()));
}

void MainWindow::applyPathPreviewToCanvas(const Core::ScanConfig &config, const QVector<Core::ScanPoint> &points)
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

void MainWindow::applyAlignmentToHeatmapView(const Core::AlignmentConfig &config)
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

void MainWindow::saveAlignmentForTaskDir(const QString &taskDir, const Core::ScanConfig &config)
{
    if (taskDir.trimmed().isEmpty()) {
        return;
    }

    Core::AlignmentConfig alignment = alignmentEditor_ && alignmentEditor_->manager()
        ? alignmentEditor_->manager()->config()
        : alignmentManager_.config();
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
    alignmentManager_.setConfig(alignment);

    const QString path = QDir(taskDir).filePath(QStringLiteral("alignment.json"));
    if (alignmentManager_.saveToFile(path)) {
        appendLog(QStringLiteral("已写入 alignment.json：%1").arg(path));
    } else {
        appendLog(QStringLiteral("写入 alignment.json 失败：%1").arg(alignmentManager_.lastError()));
    }
}

QVector<ScanPoint> MainWindow::buildMockScanPoints() const
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

    QVector<ScanPoint> points;
    points.reserve(std::min<qsizetype>(xs.size() * ys.size() * zs.size(), 50000));
    for (double z : zs) {
        for (double y : ys) {
            for (double x : xs) {
                if (points.size() >= 50000) {
                    return points;
                }
                points.push_back(ScanPoint{x, y, z});
            }
        }
    }
    return points;
}

double MainWindow::scanTableValue(int column, double fallback) const
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

void MainWindow::setScanTableValue(int column, double value)
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
