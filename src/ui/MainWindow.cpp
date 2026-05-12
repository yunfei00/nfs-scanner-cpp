#include "ui/MainWindow.h"

#include "app/AppVersion.h"
#include "analysis/FrequencyCsvParser.h"
#include "analysis/HeatmapGenerator.h"
#include "analysis/LutManager.h"
#include "core/ScanManager.h"
#include "devices/motion/SerialMotionController.h"
#include "devices/spectrum/ISpectrumAnalyzer.h"
#include "devices/spectrum/SpectrumAnalyzerFactory.h"
#include "ui/HeatmapDialog.h"
#include "ui/HeatmapView.h"

#include <QAbstractScrollArea>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleValidator>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QPixmap>
#include <QScrollBar>
#include <QSerialPortInfo>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QTime>
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

    setupUi();
    setupStatusBar();
    motionController_ = new Devices::Motion::SerialMotionController(this);
    setupMotionController();
    scanManager_ = new Core::ScanManager(this);
    setupScanManager();

    clockTimer_ = new QTimer(this);
    connect(clockTimer_, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    clockTimer_->start(1000);

    mockScanTimer_ = new QTimer(this);
    mockScanTimer_->setInterval(100);
    connect(mockScanTimer_, &QTimer::timeout, this, &MainWindow::advanceMockScan);

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
    auto *central = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(10, 10, 10, 8);
    rootLayout->setSpacing(10);

    auto *leftPanel = new QWidget(central);
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);
    leftLayout->addWidget(createSerialGroup());
    leftLayout->addWidget(createMotionControlGroup());
    leftLayout->addWidget(createMotionCommandGroup());
    leftLayout->addWidget(createStepConfigGroup());
    leftLayout->addWidget(createTestInfoGroup());
    leftLayout->addWidget(createActionGroup());
    leftLayout->addStretch(1);

    auto *rightPanel = new QWidget(central);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);
    rightLayout->addWidget(createScanAreaGroup(), 0);
    rightLayout->addWidget(createInstrumentGroup(), 0);
    rightLayout->addWidget(createResultGroup(), 0);
    rightLayout->addWidget(createHeatmapPreviewGroup(), 1);
    rightLayout->addWidget(createLogGroup(), 1);

    rootLayout->addWidget(leftPanel, 1);
    rootLayout->addWidget(rightPanel, 1);
    setCentralWidget(central);
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
    layout->addWidget(searchInstrumentButton, 1, 1, 1, 2);

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
    });
    connect(opacitySlider_, &QSlider::valueChanged, this, &MainWindow::updateOpacityLabel);
    connect(lutCombo_, &QComboBox::currentTextChanged, this, [this]() {
        if (currentColorbarImage_.isNull()) {
            updateColorbarDisplay();
        }
    });
    connect(opacitySlider_, &QSlider::valueChanged, this, [this]() {
        if (currentColorbarImage_.isNull()) {
            updateColorbarDisplay();
        }
    });

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
    statusTextLabel_ = new QLabel(statusBar_);
    statusTextLabel_->setObjectName(QStringLiteral("statusTextLabel"));
    statusTextLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    statusBar_->addPermanentWidget(statusTextLabel_, 1);
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

    connect(scanManager_, &Core::ScanManager::stateChanged, this, [this](const QString &stateText) {
        setAppState(stateText);
        updateActionButtons();
    });
    connect(scanManager_, &Core::ScanManager::logMessage, this, &MainWindow::appendLog);
    connect(scanManager_, &Core::ScanManager::progressChanged, this, [this](int current, int total) {
        updateScanProgress(current, total);
        if (heatmapView_ && currentHeatmapImage_.isNull()) {
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

    Analysis::FrequencyCsvParser parser;
    Analysis::FrequencyData loadedData;
    if (!parser.loadFile(tracePath, &loadedData)) {
        const QString message = parser.lastError();
        appendLog(QStringLiteral("加载数据失败：%1").arg(message));
        QMessageBox::warning(this, QStringLiteral("加载失败"), message);
        return;
    }

    frequencyData_ = loadedData;
    populateFrequencyControls();
    appendLog(QStringLiteral("已加载数据：trace数量=%1，频率点=%2，坐标点=%3")
                  .arg(frequencyData_.traceIds().size())
                  .arg(frequencyData_.frequencyCount())
                  .arg(frequencyData_.pointCount()));
    if (!parser.lastError().isEmpty()) {
        appendLog(parser.lastError());
    }
}

void MainWindow::populateFrequencyControls()
{
    if (traceCombo_) {
        traceCombo_->clear();
        traceCombo_->addItems(frequencyData_.traceIds());
    }

    if (frequencyCombo_) {
        frequencyCombo_->clear();
        const QVector<double> freqs = frequencyData_.freqs();
        for (double freq : freqs) {
            frequencyCombo_->addItem(formatFrequency(freq), freq);
        }
    }
}

void MainWindow::showHeatmap()
{
    if (!frequencyData_.isValid()) {
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

    const int freqIndex = frequencyCombo_ ? frequencyCombo_->currentIndex() : -1;

    const QString mode = selectedDisplayMode();
    Analysis::HeatmapGenerator generator;
    Analysis::HeatmapRenderOptions options;
    options.traceId = traceId;
    options.freqIndex = freqIndex;
    options.mode = mode;
    options.lutName = lutCombo_ ? lutCombo_->currentText() : QStringLiteral("turbo");
    options.autoRange = !autoRangeCheck_ || autoRangeCheck_->isChecked();
    options.vmin = vminSpin_ ? vminSpin_->value() : 0.0;
    options.vmax = vmaxSpin_ ? vmaxSpin_->value() : 1.0;
    options.alpha = opacitySlider_ ? std::clamp(opacitySlider_->value() * 255 / 100, 0, 255) : 220;
    options.width = 900;
    options.height = 600;

    const Analysis::HeatmapRenderResult result = generator.generate(frequencyData_, options);
    if (!result.ok || result.image.isNull()) {
        const QString message = !result.error.isEmpty() ? result.error : generator.lastError();
        appendLog(QStringLiteral("生成热力图失败：%1").arg(message));
        QMessageBox::warning(this, QStringLiteral("无法显示热力图"), message);
        return;
    }

    currentHeatmapImage_ = result.image;
    currentColorbarImage_ = result.colorbar;
    currentVmin_ = result.actualVmin;
    currentVmax_ = result.actualVmax;

    if (options.autoRange) {
        if (vminSpin_) {
            const QSignalBlocker blocker(vminSpin_);
            vminSpin_->setValue(currentVmin_);
        }
        if (vmaxSpin_) {
            const QSignalBlocker blocker(vmaxSpin_);
            vmaxSpin_->setValue(currentVmax_);
        }
    }

    if (heatmapView_) {
        heatmapView_->setOpacityPercent(opacitySlider_ ? opacitySlider_->value() : 85);
        heatmapView_->setHeatmapImage(currentHeatmapImage_);
    }
    updateColorbarDisplay();

    const QString title = QStringLiteral("%1 | %2 | %3")
                              .arg(traceId,
                                   frequencyCombo_ ? frequencyCombo_->currentText() : QStringLiteral("Frequency"),
                                   displayModeCombo_ ? displayModeCombo_->currentText() : QStringLiteral("幅度"));
    appendLog(QStringLiteral("热力图已生成：LUT=%1，范围=%2 ~ %3，透明度=%4%")
                  .arg(options.lutName,
                       QString::number(currentVmin_, 'g', 6),
                       QString::number(currentVmax_, 'g', 6),
                       QString::number(opacitySlider_ ? opacitySlider_->value() : 85)));

    HeatmapDialog dialog(currentHeatmapImage_, currentColorbarImage_, title, currentVmin_, currentVmax_, this);
    dialog.exec();
}

void MainWindow::updateColorbarDisplay()
{
    if (!colorbarLabel_) {
        return;
    }

    const QString lutName = lutCombo_ ? lutCombo_->currentText() : QStringLiteral("turbo");
    const int alpha = opacitySlider_ ? std::clamp(opacitySlider_->value() * 255 / 100, 0, 255) : 220;
    const QImage source = currentColorbarImage_.isNull()
        ? Analysis::LutManager::createColorbar(lutName, 28, 120, alpha)
        : currentColorbarImage_;

    colorbarLabel_->setPixmap(QPixmap::fromImage(source)
                                  .scaled(colorbarLabel_->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

    if (colorbarMinLabel_) {
        colorbarMinLabel_->setText(QString::number(currentVmin_, 'g', 6));
    }
    if (colorbarMaxLabel_) {
        colorbarMaxLabel_->setText(QString::number(currentVmax_, 'g', 6));
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
}

void MainWindow::clearCurrentAnalyzer()
{
    if (!currentAnalyzer_) {
        return;
    }

    if (scanManager_) {
        scanManager_->setSpectrumAnalyzer(nullptr);
    }
    if (currentAnalyzer_->isConnected()) {
        currentAnalyzer_->disconnectDevice();
    }
    currentAnalyzer_->deleteLater();
    currentAnalyzer_ = nullptr;
    updateAnalyzerButtons(false);
}

void MainWindow::connectSpectrumAnalyzer()
{
    const QString analyzerName = analyzerTypeCombo_ ? analyzerTypeCombo_->currentText() : QStringLiteral("Mock Spectrum");
    clearCurrentAnalyzer();
    currentAnalyzer_ = Devices::Spectrum::SpectrumAnalyzerFactory::create(analyzerName, this);

    connect(currentAnalyzer_, &Devices::Spectrum::ISpectrumAnalyzer::logMessage,
            this, &MainWindow::appendLog);
    connect(currentAnalyzer_, &Devices::Spectrum::ISpectrumAnalyzer::errorOccurred,
            this, [this](const QString &message) {
                appendLog(QStringLiteral("频谱仪错误：%1").arg(message));
            });
    connect(currentAnalyzer_, &Devices::Spectrum::ISpectrumAnalyzer::connectedChanged,
            this, [this](bool connected) {
                updateAnalyzerButtons(connected);
                if (deviceDiscoveryLabel_) {
                    deviceDiscoveryLabel_->setText(connected
                                                       ? QStringLiteral("发现 %1").arg(currentAnalyzer_ ? currentAnalyzer_->name() : QStringLiteral("仪表"))
                                                       : QStringLiteral("未连接频谱仪"));
                }
            });

    bool portOk = false;
    const int port = analyzerPortEdit_ ? analyzerPortEdit_->text().toInt(&portOk) : 5025;
    QVariantMap options;
    options.insert(QStringLiteral("host"), analyzerHostEdit_ ? analyzerHostEdit_->text().trimmed() : QString());
    options.insert(QStringLiteral("port"), portOk ? port : 5025);

    if (!currentAnalyzer_->connectDevice(options)) {
        const QString message = currentAnalyzer_->lastError().isEmpty()
            ? QStringLiteral("频谱仪连接失败。")
            : currentAnalyzer_->lastError();
        appendLog(QStringLiteral("频谱仪连接失败：%1").arg(message));
        QMessageBox::warning(this, QStringLiteral("频谱仪连接失败"), message);
        updateAnalyzerButtons(false);
        return;
    }

    currentSpectrumConfig_ = readSpectrumConfig();
    appendLog(QStringLiteral("频谱仪已连接：%1").arg(currentAnalyzer_->name()));
    updateAnalyzerButtons(true);
}

void MainWindow::disconnectSpectrumAnalyzer()
{
    if (!currentAnalyzer_) {
        appendLog(QStringLiteral("当前没有已创建的频谱仪连接。"));
        updateAnalyzerButtons(false);
        return;
    }

    const QString name = currentAnalyzer_->name();
    currentAnalyzer_->disconnectDevice();
    appendLog(QStringLiteral("频谱仪已断开：%1").arg(name));
    updateAnalyzerButtons(false);
}

void MainWindow::querySpectrumIdn()
{
    if (!currentAnalyzer_ || !currentAnalyzer_->isConnected()) {
        const QString message = QStringLiteral("请先连接频谱仪。");
        appendLog(message);
        QMessageBox::warning(this, QStringLiteral("频谱仪未连接"), message);
        return;
    }

    const QString idn = currentAnalyzer_->queryIdn();
    if (idn.isEmpty()) {
        const QString message = currentAnalyzer_->lastError().isEmpty()
            ? QStringLiteral("IDN 查询无返回。")
            : currentAnalyzer_->lastError();
        appendLog(QStringLiteral("IDN 查询失败：%1").arg(message));
        QMessageBox::warning(this, QStringLiteral("IDN 查询失败"), message);
        return;
    }

    appendLog(QStringLiteral("频谱仪 IDN：%1").arg(idn));
}

void MainWindow::applySpectrumConfig()
{
    if (!currentAnalyzer_ || !currentAnalyzer_->isConnected()) {
        const QString message = QStringLiteral("请先连接仪表。");
        appendLog(message);
        QMessageBox::warning(this, QStringLiteral("频谱仪未连接"), message);
        return;
    }

    currentSpectrumConfig_ = readSpectrumConfig();
    if (!currentAnalyzer_->configure(currentSpectrumConfig_)) {
        const QString message = currentAnalyzer_->lastError().isEmpty()
            ? QStringLiteral("应用仪表配置失败。")
            : currentAnalyzer_->lastError();
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

    syncStepInputsToTable();

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
    config.outputDir = resultDirEdit_ && !resultDirEdit_->text().trimmed().isEmpty()
        ? resultDirEdit_->text().trimmed()
        : QStringLiteral("data/scans");

    remainingText_ = QStringLiteral("--");
    estimatedFinishText_ = QStringLiteral("--");
    updateScanProgress(0, 1);
    currentSpectrumConfig_ = readSpectrumConfig();
    scanManager_->setSpectrumConfig(currentSpectrumConfig_);
    scanManager_->setSpectrumAnalyzer(currentAnalyzer_ && currentAnalyzer_->isConnected() ? currentAnalyzer_ : nullptr);
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
