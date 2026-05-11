#include "ui/MainWindow.h"

#include <QAbstractScrollArea>
#include <QButtonGroup>
#include <QComboBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSizePolicy>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QTime>
#include <QVBoxLayout>
#include <QWidget>

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
    setWindowTitle(QStringLiteral("NFS Scanner v1.0.0 - 近场扫描系统"));
    resize(1600, 900);

    setupUi();
    setupStatusBar();

    clockTimer_ = new QTimer(this);
    connect(clockTimer_, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    clockTimer_->start(1000);

    mockScanTimer_ = new QTimer(this);
    mockScanTimer_->setInterval(100);
    connect(mockScanTimer_, &QTimer::timeout, this, &MainWindow::advanceMockScan);

    updateActionButtons();
    updateStatusBar();
    appendLog(QStringLiteral("系统初始化完成，当前为 UI Mock 模式，未连接真实硬件。"));
}

MainWindow::~MainWindow() = default;

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
    closeSerialButton_->setEnabled(false);

    layout->addWidget(new QLabel(QStringLiteral("端口号"), group), 0, 0);
    layout->addWidget(serialPortCombo_, 0, 1, 1, 2);
    layout->addWidget(new QLabel(QStringLiteral("波特率"), group), 1, 0);
    layout->addWidget(baudRateCombo_, 1, 1, 1, 2);
    layout->addWidget(openSerialButton_, 2, 0);
    layout->addWidget(closeSerialButton_, 2, 1);
    layout->addWidget(refreshSerialButton_, 2, 2);

    connect(refreshSerialButton_, &QPushButton::clicked, this, &MainWindow::refreshSerialPorts);
    connect(openSerialButton_, &QPushButton::clicked, this, &MainWindow::openSerialPort);
    connect(closeSerialButton_, &QPushButton::clicked, this, &MainWindow::closeSerialPort);

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
    connect(versionButton, &QPushButton::clicked, this, [this]() {
        appendLog(QStringLiteral("Mock GRBL Controller v1.0"));
    });
    connect(helpButton, &QPushButton::clicked, this, [this]() {
        appendLog(QStringLiteral("支持命令：$H、?、$I、G1X..Y..Z..F.."));
    });
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

    auto *projectEdit = new QLineEdit(group);
    projectEdit->setPlaceholderText(QStringLiteral("请输入项目名称"));
    auto *testEdit = new QLineEdit(group);
    testEdit->setPlaceholderText(QStringLiteral("请输入测试名称"));

    layout->addRow(QStringLiteral("项目名称"), projectEdit);
    layout->addRow(QStringLiteral("测试名称"), testEdit);

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
    group->setMaximumHeight(132);
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

    const QList<double> defaults{0.0, 0.0, 0.0, 10.0, 10.0, 1.0, 0.5, 0.5, 0.5};
    for (int column = 0; column < defaults.size(); ++column) {
        setScanTableValue(column, defaults.at(column));
    }

    layout->addWidget(scanTable_);
    return group;
}

QGroupBox *MainWindow::createInstrumentGroup()
{
    auto *group = new QGroupBox(QStringLiteral("仪表区域"), this);
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(10, 12, 10, 10);

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
    addParameter(0, 0, QStringLiteral("起始频率"), QStringLiteral("1000.00"), freqUnits);
    addParameter(1, 0, QStringLiteral("终止频率"), QStringLiteral("3000.00"), freqUnits);
    addParameter(2, 0, QStringLiteral("RBW"), QStringLiteral("10.00"), rbwUnits);
    addParameter(3, 0, QStringLiteral("Scale"), QStringLiteral("10.00"), QStringList{}, QStringLiteral("dB/div"));

    addParameter(0, 4, QStringLiteral("中心频率"), QStringLiteral("2000.00"), freqUnits);
    addParameter(1, 4, QStringLiteral("Span"), QStringLiteral("2000.00"), freqUnits);

    auto *pointsLabel = new QLabel(QStringLiteral("扫描点数"), znaPage);
    auto *pointsEdit = createIntegerEdit(QStringLiteral("1001"), znaPage);
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
    return group;
}

QGroupBox *MainWindow::createResultGroup()
{
    auto *group = new QGroupBox(QStringLiteral("结果区域"), this);
    auto *layout = new QHBoxLayout(group);
    layout->setContentsMargins(10, 12, 10, 10);
    layout->setSpacing(8);

    resultDirEdit_ = new QLineEdit(QStringLiteral("output"), group);
    auto *viewButton = new QPushButton(QStringLiteral("查看"), group);
    auto *heatmapButton = new QPushButton(QStringLiteral("显示热力图"), group);

    layout->addWidget(new QLabel(QStringLiteral("结果"), group));
    layout->addWidget(resultDirEdit_, 1);
    layout->addWidget(viewButton);
    layout->addWidget(heatmapButton);

    connect(viewButton, &QPushButton::clicked, this, [this]() {
        appendLog(QStringLiteral("查看结果目录：%1").arg(resultDirEdit_->text()));
    });
    connect(heatmapButton, &QPushButton::clicked, this, [this]() {
        appendLog(QStringLiteral("显示热力图功能待接入。"));
    });

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

void MainWindow::refreshSerialPorts()
{
    serialPortCombo_->clear();
    serialPortCombo_->addItems(QStringList{QStringLiteral("COM1"), QStringLiteral("COM2"), QStringLiteral("COM3")});
    appendLog(QStringLiteral("串口列表已刷新：COM1、COM2、COM3"));
}

void MainWindow::openSerialPort()
{
    if (serialPortCombo_->count() == 0) {
        refreshSerialPorts();
    }

    openSerialButton_->setEnabled(false);
    closeSerialButton_->setEnabled(true);
    setAppState(QStringLiteral("串口已连接"));
    appendLog(QStringLiteral("串口已连接：%1，%2")
                  .arg(serialPortCombo_->currentText(), baudRateCombo_->currentText()));
}

void MainWindow::closeSerialPort()
{
    openSerialButton_->setEnabled(true);
    closeSerialButton_->setEnabled(false);
    setAppState(QStringLiteral("就绪"));
    appendLog(QStringLiteral("串口已关闭。"));
}

void MainWindow::jogAxis(const QString &axis, double direction)
{
    if (axis == QStringLiteral("X")) {
        currentX_ += direction * jogStep_;
    } else if (axis == QStringLiteral("Y")) {
        currentY_ += direction * jogStep_;
    } else if (axis == QStringLiteral("Z")) {
        currentZ_ += direction * jogStep_;
    }

    updateStatusBar();
    appendLog(QStringLiteral("点动 %1%2 %3 mm，当前位置 %4")
                  .arg(axis,
                       direction > 0.0 ? QStringLiteral("+") : QStringLiteral("-"),
                       mmText(jogStep_),
                       positionText(currentX_, currentY_, currentZ_)));
}

void MainWindow::resetPosition()
{
    currentX_ = 0.0;
    currentY_ = 0.0;
    currentZ_ = 0.0;
    updateStatusBar();
    appendLog(QStringLiteral("执行复位，当前位置已清零。"));
}

void MainWindow::queryPosition()
{
    appendLog(QStringLiteral("当前位置 %1").arg(positionText(currentX_, currentY_, currentZ_)));
}

void MainWindow::executeAbsoluteMove()
{
    bool xOk = false;
    bool yOk = false;
    bool zOk = false;
    bool feedOk = false;
    const double x = absoluteXEdit_->text().toDouble(&xOk);
    const double y = absoluteYEdit_->text().toDouble(&yOk);
    const double z = absoluteZEdit_->text().toDouble(&zOk);
    const int feed = feedEdit_->text().toInt(&feedOk);

    if (!xOk || !yOk || !zOk || !feedOk) {
        appendLog(QStringLiteral("绝对坐标参数无效，G1 命令未执行。"));
        return;
    }

    currentX_ = x;
    currentY_ = y;
    currentZ_ = z;
    updateStatusBar();
    appendLog(QStringLiteral("执行 G1 X%1 Y%2 Z%3 F%4，当前位置 %5")
                  .arg(mmText(currentX_),
                       mmText(currentY_),
                       mmText(currentZ_),
                       QString::number(feed),
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
    if (mockScanTimer_->isActive()) {
        return;
    }

    if (appState_ == QStringLiteral("已暂停") && !mockScanPoints_.isEmpty() && scanIndex_ < mockScanPoints_.size()) {
        setAppState(QStringLiteral("扫描中"));
        appendLog(QStringLiteral("扫描继续。"));
        mockScanTimer_->start();
        updateActionButtons();
        return;
    }

    syncStepInputsToTable();
    mockScanPoints_ = buildMockScanPoints();
    scanIndex_ = 0;

    if (mockScanPoints_.isEmpty()) {
        remainingText_ = QStringLiteral("--");
        estimatedFinishText_ = QStringLiteral("--");
        setAppState(QStringLiteral("就绪"));
        appendLog(QStringLiteral("扫描区域参数无效，未生成扫描点。"));
        updateActionButtons();
        return;
    }

    remainingText_ = QString::number(mockScanPoints_.size());
    estimatedFinishText_ = QStringLiteral("%1 秒").arg(std::ceil(mockScanPoints_.size() / 10.0), 0, 'f', 0);
    setAppState(QStringLiteral("扫描中"));
    appendLog(QStringLiteral("扫描开始，共 %1 个点。").arg(mockScanPoints_.size()));
    mockScanTimer_->start();
    updateActionButtons();
}

void MainWindow::pauseScan()
{
    if (mockScanTimer_->isActive()) {
        mockScanTimer_->stop();
    }
    setAppState(QStringLiteral("已暂停"));
    appendLog(QStringLiteral("扫描已暂停。"));
    updateActionButtons();
}

void MainWindow::stopScan()
{
    if (mockScanTimer_->isActive()) {
        mockScanTimer_->stop();
    }
    scanIndex_ = 0;
    mockScanPoints_.clear();
    remainingText_ = QStringLiteral("--");
    estimatedFinishText_ = QStringLiteral("--");
    setAppState(QStringLiteral("已停止"));
    appendLog(QStringLiteral("扫描已停止。"));
    updateActionButtons();
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

    const bool running = mockScanTimer_ && mockScanTimer_->isActive();
    const bool paused = appState_ == QStringLiteral("已暂停");
    startScanButton_->setText(paused ? QStringLiteral("继续") : QStringLiteral("开始"));
    startScanButton_->setEnabled(!running);
    pauseScanButton_->setEnabled(running);
    stopScanButton_->setEnabled(running || paused);
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
