#include "ui/MainWindow.h"

#include "app/AppVersion.h"
#include "core/AlignmentManager.h"
#include "core/PreScanChecklist.h"
#include "core/DeviceManager.h"
#include "core/ScanManager.h"
#include "core/ScanHardware.h"
#include "diagnostics/DiagnosticPackageExporter.h"
#include "diagnostics/HardwareDiagnostics.h"
#include "ui/HardwareDebugDialog.h"
#include "license/LicenseManager.h"
#include "project/ProjectManager.h"
#include "report/ReportData.h"
#include "report/ReportGenerator.h"
#include "devices/camera/ICamera.h"
#include "devices/motion/SerialMotionController.h"
#include "ui/AlignmentEditor.h"
#include "ui/DeviceStatusBar.h"
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
#include <QStandardPaths>
#include <QDoubleValidator>
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
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
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

    setupUi();
    setupStatusBar();

    connect(deviceManager_, &Core::DeviceManager::logMessage, this, &MainWindow::appendLog);
    connect(projectManager_, &Project::ProjectManager::logMessage, this, &MainWindow::appendLog);
    connect(projectManager_, &Project::ProjectManager::projectChanged, this, [this]() {
        updateProjectStatusDisplay();
        if (analysisPage_) {
            analysisPage_->refreshProjectPaths();
        }
        if (reportPage_) {
            reportPage_->refreshTaskList();
        }
    });

    scanManager_ = new Core::ScanManager(this);
    setupScanManager();
    setupScanPageBindings();
    setupDevicePageBindings();
    setupAnalysisPageBindings();
    setupReportPageBindings();

    clockTimer_ = new QTimer(this);
    connect(clockTimer_, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    clockTimer_->start(1000);

    if (deviceStatusBar_) {
        deviceStatusBar_->bindDeviceManager(deviceManager_);
        deviceStatusBar_->bindProjectManager(projectManager_);
        deviceStatusBar_->bindLicenseManager(licenseManager_);
    }

    updateStatusBar();
    appendLog(QStringLiteral("系统初始化完成，默认启用模拟模式，未发送真实串口命令。"));
}

MainWindow::~MainWindow()
{
    if (devicePage_) {
        devicePage_->clearCurrentAnalyzer();
    }
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
    toolsMenu->addAction(QStringLiteral("导出图片"), this, [this]() {
        if (analysisPage_) {
            analysisPage_->showHeatmap();
        }
    });
    toolsMenu->addAction(QStringLiteral("导出报告 (HTML)"), this, [this]() { exportCurrentReport(QStringLiteral("html")); });
    toolsMenu->addAction(QStringLiteral("导出分析配置 JSON"), this, [this]() {
        if (analysisPage_) {
            analysisPage_->exportAnalysisConfigJson();
        }
    });
    toolsMenu->addAction(QStringLiteral("打开数据目录"), this, [this]() {
        const QString path = analysisPage_ ? analysisPage_->resultDir() : QString();
        if (!path.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    });

    auto *deviceMenu = menuBar()->addMenu(QStringLiteral("设备(&D)"));
    deviceMenu->addAction(QStringLiteral("刷新设备"), this, [this]() {
        if (deviceManager_) {
            deviceManager_->refreshDevices();
        }
        if (devicePage_) {
            devicePage_->refreshSerialPorts();
        }
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
    helpMenu->addAction(QStringLiteral("导出诊断包"), this, &MainWindow::exportDiagnosticPackage);
    helpMenu->addAction(QStringLiteral("硬件调试面板"), this, &MainWindow::showHardwareDebugDialog);
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
    scanDockLayout->setContentsMargins(0, 0, 0, 0);
    scanDockLayout->setSpacing(6);
    scanDockLayout->addWidget(scanPage_->paramPanel());

    auto *utilityRow = new QWidget(scanDockPage);
    auto *utilityLayout = new QHBoxLayout(utilityRow);
    utilityLayout->setContentsMargins(4, 0, 4, 4);
    utilityLayout->setSpacing(7);
    auto *clearLogButton = new QPushButton(QStringLiteral("清除日志"), utilityRow);
    auto *searchInstrumentButton = new QPushButton(QStringLiteral("搜索仪表"), utilityRow);
    utilityLayout->addWidget(clearLogButton);
    utilityLayout->addWidget(searchInstrumentButton);
    utilityLayout->addStretch(1);
    scanDockLayout->addWidget(utilityRow);
    connect(clearLogButton, &QPushButton::clicked, this, [this]() {
        if (logEdit_) {
            logEdit_->clear();
        }
    });
    connect(searchInstrumentButton, &QPushButton::clicked, this, [this]() {
        if (devicePage_ && devicePage_->deviceDiscoveryLabel()) {
            devicePage_->deviceDiscoveryLabel()->setText(QStringLiteral("发现 Mock 仪表"));
        }
        appendLog(QStringLiteral("设备发现完成：发现 Mock 仪表。"));
    });
    scanDockLayout->addStretch(1);
    paramDockStack_->addWidget(scanDockPage);

    auto *deviceDockPage = new QWidget(paramDockStack_);
    auto *deviceDockLayout = new QVBoxLayout(deviceDockPage);
    deviceDockLayout->setContentsMargins(4, 4, 4, 4);
    deviceDockLayout->addWidget(devicePage_->paramPanel());
    deviceDockLayout->addStretch(1);
    paramDockStack_->addWidget(deviceDockPage);

    auto *analysisDockPage = new QWidget(paramDockStack_);
    auto *analysisDockLayout = new QVBoxLayout(analysisDockPage);
    analysisDockLayout->setContentsMargins(4, 4, 4, 4);
    analysisDockLayout->addWidget(analysisPage_->paramPanel());
    analysisDockLayout->addStretch(1);
    paramDockStack_->addWidget(analysisDockPage);

    auto *reportDockPage = new QWidget(paramDockStack_);
    auto *reportDockLayout = new QVBoxLayout(reportDockPage);
    reportDockLayout->setContentsMargins(4, 4, 4, 4);
    reportDockLayout->addWidget(reportPage_->paramPanel());
    reportDockLayout->addStretch(1);
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
    if (reportPage_) {
        reportPage_->exportReport(format);
    }
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(this, QStringLiteral("关于"),
                       QStringLiteral("%1 v%2\n\n近场扫描系统 Qt C++ 客户端。").arg(QStringLiteral(APP_NAME), QStringLiteral(APP_VERSION)));
}

void MainWindow::showDiagnosticsDialog()
{
    Diagnostics::HardwareDiagnosticsOptions options;
    options.projectManager = projectManager_;
    options.licenseManager = licenseManager_;
    options.deviceManager = deviceManager_;
    if (projectManager_ && projectManager_->hasOpenProject()) {
        options.projectPath = projectManager_->currentProject().rootPath;
    }

    const QString summary = Diagnostics::HardwareDiagnostics::buildMarkdownSummary(options);
    QString exportPath;
    const bool exported = Diagnostics::HardwareDiagnostics::exportMarkdownReport(options, &exportPath);

    QString text = summary;
    if (exported) {
        text.append(QStringLiteral("\n\n已导出: %1").arg(exportPath));
        appendLog(QStringLiteral("设备诊断已导出: %1").arg(exportPath));
    }

    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("设备诊断"));
    box.setText(QStringLiteral("诊断摘要已生成，详情见导出文件。"));
    box.setDetailedText(text);
    box.setIcon(QMessageBox::Information);
    box.exec();
}

void MainWindow::showHardwareDebugDialog()
{
    HardwareDebugDialog dialog(deviceManager_, motionController_, this);
    dialog.exec();
}

void MainWindow::exportDiagnosticPackage()
{
    Diagnostics::DiagnosticPackageOptions options;
    options.deviceManager = deviceManager_;
    options.licenseManager = licenseManager_;
    options.projectManager = projectManager_;
    options.selfCheckSummary = QStringLiteral("Run NFSScannerSelfCheck.exe for latest automated results.");
    QString outputDir;
    if (Diagnostics::DiagnosticPackageExporter::exportPackage(options, &outputDir)) {
        appendLog(QStringLiteral("诊断包已导出: %1").arg(outputDir));
        QMessageBox::information(this, QStringLiteral("诊断包"), QStringLiteral("已导出至:\n%1").arg(outputDir));
    } else {
        QMessageBox::warning(this, QStringLiteral("诊断包"), QStringLiteral("导出失败。"));
    }
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

void MainWindow::setupScanManager()
{
    if (!scanManager_) {
        return;
    }

    scanManager_->setSpectrumDeviceHost(deviceManager_ ? deviceManager_->spectrumDeviceHost() : nullptr);
    scanManager_->setSpectrumDeviceThread(deviceManager_ ? deviceManager_->spectrumDeviceThread() : nullptr);
    scanManager_->setDeviceManager(deviceManager_);

    connect(scanManager_, &Core::ScanManager::stateChanged, this, [this](const QString &stateText) {
        setAppState(stateText);
    });
    connect(scanManager_, &Core::ScanManager::logMessage, this, &MainWindow::appendLog);
    connect(scanManager_, &Core::ScanManager::progressChanged, this, [this](int current, int total) {
        if (scanPage_) {
            scanPage_->updateScanProgress(current, total);
        }
        if (heatmapView_ && analysisPage_ && analysisPage_->heatmapImage().isNull()) {
            heatmapView_->setScanProgress(current, total);
        }
    });
    connect(scanManager_, &Core::ScanManager::currentPointChanged, this, [this](int, int, double x, double y, double z) {
        currentX_ = x;
        currentY_ = y;
        currentZ_ = z;
        if (scanPage_) {
            scanPage_->setCurrentPosition(x, y, z);
        }
        updateStatusBar();
    });
    connect(scanManager_, &Core::ScanManager::estimatedChanged, this, [this](int remainingCount, int estimatedSeconds) {
        remainingText_ = QString::number(remainingCount);
        estimatedFinishText_ = QStringLiteral("%1s").arg(estimatedSeconds);
        updateStatusBar();
    });
    connect(scanManager_, &Core::ScanManager::taskDirChanged, this, [this](const QString &taskDir) {
        if (analysisPage_) {
            analysisPage_->setResultDir(taskDir);
        }
    });
    connect(scanManager_, &Core::ScanManager::scanFinished, this, [this](const QString &taskDir) {
        remainingText_ = QStringLiteral("0");
        estimatedFinishText_ = QStringLiteral("--");
        if (analysisPage_) {
            analysisPage_->setResultDir(taskDir);
            analysisPage_->loadFrequencyData();
        }
        if (reportPage_) {
            reportPage_->refreshTaskList();
        }
        appendLog(QStringLiteral("扫描完成，数据已保存：%1").arg(taskDir));
        appendLog(QStringLiteral("扫描完成，可点击“显示热力图”。"));
        updateStatusBar();
    });
    connect(scanManager_, &Core::ScanManager::scanError, this, [this](const QString &message) {
        remainingText_ = QStringLiteral("--");
        estimatedFinishText_ = QStringLiteral("--");
        appendLog(QStringLiteral("扫描错误：%1").arg(message));
        updateStatusBar();
        QMessageBox::warning(this, QStringLiteral("扫描错误"), message);
    });
}

void MainWindow::setupScanPageBindings()
{
    if (!scanPage_) {
        return;
    }

    scanPage_->bind(scanManager_,
                    heatmapView_,
                    scanPage_->alignmentEditor(),
                    &alignmentManager_,
                    projectManager_,
                    logEdit_,
                    this);

    scanPage_->setFeedProvider([this]() {
        return devicePage_ ? devicePage_->feedValue() : 1000.0;
    });
    scanPage_->setMockModeChecker([this]() {
        return !devicePage_ || devicePage_->isMockMode();
    });
    scanPage_->setMotionReadyChecker([this]() {
        return motionController_ && motionController_->isOpen();
    });
    scanPage_->setOnScanPageChecker([this]() { return currentPage_ == AppPage::Scan; });
    scanPage_->setResultDirProvider([this]() {
        return analysisPage_ ? analysisPage_->resultDir() : QString();
    });
    scanPage_->setPreScanHandler([this](const Core::ScanConfig &config, int pointCount, const QString &plannerError) {
        Core::PreScanChecklistContext context;
        context.scanConfig = config;
        context.deviceManager = deviceManager_;
        context.projectExists = projectManager_ && projectManager_->hasOpenProject();
        context.mockMode = devicePage_ && devicePage_->isMockMode();
        if (deviceManager_) {
            const bool motionMock = context.mockMode || deviceManager_->motionMockMode();
            const bool spectrumMock = context.mockMode
                || deviceManager_->hardwareConfig().spectrum.type.compare(QStringLiteral("mock"), Qt::CaseInsensitive) == 0;
            context.hardwareMode = Core::inferHardwareMode(motionMock, spectrumMock);
            context.hardwareProfileName = deviceManager_->hardwareProfileName();
        }
        context.pointCount = pointCount;
        context.plannerError = plannerError;
        context.hasAlignment = alignmentManager_.config().enabled;

        if (licenseManager_) {
            const auto status = licenseManager_->status();
            context.licenseDemo = status == License::LicenseStatus::Demo;
            context.licenseValid = status == License::LicenseStatus::Valid
                || status == License::LicenseStatus::Demo;
        }

        const QString outputDir = config.outputDir.trimmed().isEmpty()
            ? QStringLiteral("data/scans")
            : config.outputDir.trimmed();
        QDir dir(outputDir);
        context.outputDirWritable = dir.exists() ? QFileInfo(outputDir).isWritable() : dir.mkpath(QStringLiteral("."));

        const Core::PreScanChecklistResult checklist = Core::PreScanChecklist::evaluate(context);
        appendLog(QStringLiteral("扫描前检查完成。"));
        appendLog(checklist.summaryText());

        if (checklist.hasErrors()) {
            QMessageBox::critical(this,
                                  QStringLiteral("扫描前检查未通过"),
                                  checklist.summaryText());
            return false;
        }
        if (checklist.hasWarnings()) {
            const auto answer = QMessageBox::warning(this,
                                                     QStringLiteral("扫描前检查警告"),
                                                     checklist.summaryText() + QStringLiteral("\n\n是否继续扫描？"),
                                                     QMessageBox::Yes | QMessageBox::No,
                                                     QMessageBox::No);
            return answer == QMessageBox::Yes;
        }
        return true;
    });
    scanPage_->setScanLaunchHandler([this](Core::ScanManager *manager, const Core::ScanConfig &config) {
        remainingText_ = QStringLiteral("--");
        estimatedFinishText_ = QStringLiteral("--");
        if (!devicePage_) {
            Q_UNUSED(config)
            return true;
        }

        const auto spectrumConfig = devicePage_->readSpectrumConfig();
        manager->setSpectrumConfig(spectrumConfig);
        auto *analyzer = devicePage_->currentAnalyzer();
        manager->setSpectrumAnalyzer(analyzer && analyzer->isConnected() ? analyzer : nullptr);
        manager->setMotionController(motionController_);
        manager->setUseRealMotion(!devicePage_->isMockMode());
        manager->setDeviceManager(deviceManager_);
        if (deviceManager_) {
            manager->setHardwareProfileName(deviceManager_->hardwareProfileName());
            const bool motionMock = devicePage_->isMockMode() || deviceManager_->motionMockMode();
            const bool spectrumMock = devicePage_->isMockMode()
                || deviceManager_->hardwareConfig().spectrum.type.compare(QStringLiteral("mock"), Qt::CaseInsensitive) == 0;
            if (scanPage_) {
                scanPage_->setHardwareModeText(Core::hardwareModeToString(Core::inferHardwareMode(motionMock, spectrumMock)));
            }
        }
        Core::ScanAcquisitionOptions acquisitionOptions;
        acquisitionOptions.timeoutMs = devicePage_->acquisitionTimeoutMs();
        acquisitionOptions.retryCount = devicePage_->acquisitionRetryCount();
        acquisitionOptions.stopOnError = devicePage_->stopOnAcquisitionError();
        manager->setAcquisitionOptions(acquisitionOptions);
        if (devicePage_->isNonMockAnalyzerSelected()
            && (!analyzer || !analyzer->isConnected())) {
            appendLog(QStringLiteral("真实仪表未连接，将使用 Mock Spectrum 数据完成扫描。"));
        }
        Q_UNUSED(config)
        return true;
    });

    connect(scanPage_, &ScanPage::logMessage, this, &MainWindow::appendLog);
    connect(scanPage_, &ScanPage::requestSwitchToScanPage, this, [this]() {
        switchToPage(AppPage::Scan);
    });
    connect(scanPage_, &ScanPage::mockScanPositionChanged, this, [this](double x, double y, double z) {
        currentX_ = x;
        currentY_ = y;
        currentZ_ = z;
        updateStatusBar();
    });
    connect(scanPage_, &ScanPage::mockScanProgressChanged, this, [this](int remainingCount, int estimatedSeconds) {
        remainingText_ = QString::number(remainingCount);
        estimatedFinishText_ = estimatedSeconds > 0
            ? QStringLiteral("%1 秒").arg(estimatedSeconds)
            : QStringLiteral("--");
        updateStatusBar();
    });
    connect(scanPage_, &ScanPage::mockScanStateChanged, this, &MainWindow::setAppState);
    connect(scanPage_, &ScanPage::mockScanFinished, this, [this]() {
        remainingText_ = QStringLiteral("0");
        estimatedFinishText_ = QStringLiteral("--");
        updateStatusBar();
    });

    if (auto *editor = scanPage_->alignmentEditor()) {
        connect(editor, &AlignmentEditor::mockCaptureRequested, this, [this, editor]() {
            if (!deviceManager_) {
                return;
            }
            if (!deviceManager_->camera() || !deviceManager_->camera()->isConnected()) {
                deviceManager_->connectCamera(false);
            }
            if (!deviceManager_->camera()) {
                appendLog(QStringLiteral("Mock 相机不可用。"));
                return;
            }
            const QImage frame = deviceManager_->camera()->captureFrame();
            const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
            const QString path = QDir(dir).filePath(QStringLiteral("nfs_mock_camera_bg.png"));
            if (!frame.save(path)) {
                appendLog(QStringLiteral("Mock 相机截图保存失败。"));
                return;
            }
            editor->captureMockBackground(frame, path);
            appendLog(QStringLiteral("Mock 相机背景已保存：%1").arg(path));
        });
    }
}

void MainWindow::setupDevicePageBindings()
{
    if (!devicePage_) {
        return;
    }

    devicePage_->bind(deviceManager_, scanManager_, motionController_, logEdit_, this);
    devicePage_->setStateSetter([this](const QString &state) { setAppState(state); });
    devicePage_->setPositionChangedHandler([this](double x, double y, double z) {
        currentX_ = x;
        currentY_ = y;
        currentZ_ = z;
        if (scanPage_) {
            scanPage_->setCurrentPosition(x, y, z);
        }
        updateStatusBar();
    });
    devicePage_->setDwellMsProvider([this]() {
        return scanPage_ ? scanPage_->readScanConfigFromUi().dwellMs : 100;
    });

    connect(devicePage_, &DevicePage::logMessage, this, &MainWindow::appendLog);
    connect(devicePage_, &DevicePage::analyzerConnectedChanged, this, [this](bool) {
        if (deviceStatusBar_) {
            deviceStatusBar_->refresh();
        }
    });
}

void MainWindow::setupAnalysisPageBindings()
{
    if (!analysisPage_) {
        return;
    }

    analysisPage_->bind(heatmapView_, projectManager_, this);
    connect(analysisPage_, &AnalysisPage::logMessage, this, &MainWindow::appendLog);
}

void MainWindow::setupReportPageBindings()
{
    if (!reportPage_) {
        return;
    }

    reportPage_->bind(scanPage_, analysisPage_, devicePage_, projectManager_, deviceManager_, licenseManager_, this);
    connect(reportPage_, &ReportPage::logMessage, this, &MainWindow::appendLog);
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
    if (deviceStatusBar_) {
        deviceStatusBar_->setScanStateText(state);
    }
    updateStatusBar();
}

void MainWindow::startScan()
{
    if (scanPage_) {
        scanPage_->startScan();
    }
}

void MainWindow::pauseScan()
{
    if (scanPage_) {
        scanPage_->pauseScan();
    }
}

void MainWindow::stopScan()
{
    if (scanPage_) {
        scanPage_->stopScan();
        remainingText_ = QStringLiteral("--");
        estimatedFinishText_ = QStringLiteral("--");
        updateStatusBar();
    }
}

} // namespace NFSScanner::UI
