#include "ui/pages/DevicePage.h"

#include "core/DeviceManager.h"
#include "core/ScanManager.h"
#include "devices/camera/ICamera.h"
#include "devices/motion/IMotionController.h"
#include "devices/motion/SerialMotionController.h"
#include "devices/spectrum/ISpectrumAnalyzer.h"
#include "devices/spectrum/SpectrumAnalyzerFactory.h"
#include "ui/UiFormUtils.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSerialPortInfo>
#include <QSpinBox>
#include <QTabWidget>
#include <QTime>
#include <QVBoxLayout>
#include <QVariantMap>

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

DevicePage::DevicePage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("devicePage"));
    auto *pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    contentHost_ = new QWidget(scroll);
    contentHost_->setObjectName(QStringLiteral("devicePageContent"));
    scroll->setWidget(contentHost_);
    pageLayout->addWidget(scroll);

    buildContentHost();
    paramPanel_ = buildParamPanel();
}

QWidget *DevicePage::contentHost() const
{
    return contentHost_;
}

QWidget *DevicePage::paramPanel() const
{
    return paramPanel_;
}

void DevicePage::bind(NFSScanner::Core::DeviceManager *deviceManager,
                      NFSScanner::Core::ScanManager *scanManager,
                      NFSScanner::Devices::Motion::SerialMotionController *motionController,
                      QPlainTextEdit *logEdit,
                      QWidget *messageBoxParent)
{
    deviceManager_ = deviceManager;
    scanManager_ = scanManager;
    motionController_ = motionController;
    logEdit_ = logEdit;
    messageBoxParent_ = messageBoxParent;

    if (!motionController_) {
        return;
    }

    connect(motionController_, &Devices::Motion::SerialMotionController::connectedChanged, this, [this](bool connected) {
        updateSerialButtons(connected);
        if (stateSetter_) {
            stateSetter_(connected ? QStringLiteral("串口已连接") : QStringLiteral("串口已关闭"));
        }
    });
    connect(motionController_, &Devices::Motion::IMotionController::positionChanged, this, [this](double x, double y, double z) {
        currentX_ = x;
        currentY_ = y;
        currentZ_ = z;
        notifyPositionChanged();
    });
    connect(motionController_, &Devices::Motion::SerialMotionController::statusChanged, this, [this](const QString &state) {
        if (stateSetter_) {
            stateSetter_(state);
        }
    });
    connect(motionController_, &Devices::Motion::SerialMotionController::logMessage, this, &DevicePage::appendLog);
    connect(motionController_, &Devices::Motion::SerialMotionController::rawLineReceived, this, [this](const QString &line) {
        appendLog(QStringLiteral("接收：%1").arg(line));
    });
    connect(motionController_, &Devices::Motion::IMotionController::errorOccurred, this, [this](const QString &message) {
        appendLog(QStringLiteral("错误：%1").arg(message));
    });
}

void DevicePage::setStateSetter(StateSetter setter)
{
    stateSetter_ = std::move(setter);
}

void DevicePage::setPositionChangedHandler(PositionChangedHandler handler)
{
    positionChangedHandler_ = std::move(handler);
}

void DevicePage::setDwellMsProvider(DwellMsProvider provider)
{
    dwellMsProvider_ = std::move(provider);
}

bool DevicePage::isMockMode() const
{
    return !mockModeCheck_ || mockModeCheck_->isChecked();
}

double DevicePage::feedValue() const
{
    if (!feedEdit_ || feedEdit_->text().trimmed().isEmpty()) {
        return 1000.0;
    }

    bool ok = false;
    const double feed = feedEdit_->text().toDouble(&ok);
    return ok && feed > 0.0 ? feed : 1000.0;
}

NFSScanner::Devices::Spectrum::ISpectrumAnalyzer *DevicePage::currentAnalyzer() const
{
    return currentAnalyzer_;
}

NFSScanner::Devices::Spectrum::SpectrumConfig DevicePage::currentSpectrumConfig() const
{
    return currentSpectrumConfig_;
}

NFSScanner::Devices::Spectrum::SpectrumConfig DevicePage::readSpectrumConfig() const
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
    const int dwellMs = dwellMsProvider_ ? dwellMsProvider_() : 100;
    config.sweepTimeSec = std::max(0.05, static_cast<double>(dwellMs) / 1000.0);
    config.traceId = QStringLiteral("Trc1_S21");
    return config;
}

int DevicePage::acquisitionTimeoutMs() const
{
    return acquisitionTimeoutSpin_ ? acquisitionTimeoutSpin_->value() : 10000;
}

int DevicePage::acquisitionRetryCount() const
{
    return acquisitionRetrySpin_ ? acquisitionRetrySpin_->value() : 1;
}

bool DevicePage::stopOnAcquisitionError() const
{
    return !stopOnErrorCheck_ || stopOnErrorCheck_->isChecked();
}

bool DevicePage::isNonMockAnalyzerSelected() const
{
    return analyzerTypeCombo_
        && analyzerTypeCombo_->currentText() != QStringLiteral("Mock Spectrum");
}

QLabel *DevicePage::deviceDiscoveryLabel() const
{
    return deviceDiscoveryLabel_;
}

void DevicePage::refreshSerialPorts()
{
    if (!serialPortCombo_) {
        return;
    }

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

void DevicePage::clearCurrentAnalyzer()
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
    emit analyzerConnectedChanged(false);
}

void DevicePage::buildContentHost()
{
    auto *hostLayout = new QVBoxLayout(contentHost_);
    hostLayout->setContentsMargins(4, 4, 4, 4);
    hostLayout->setSpacing(8);
    hostLayout->addWidget(createSerialGroup());
    hostLayout->addWidget(createMotionControlGroup());
    hostLayout->addWidget(createMotionCommandGroup());
    hostLayout->addStretch(1);
}

QWidget *DevicePage::buildParamPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(createInstrumentGroup());
    return panel;
}

QGroupBox *DevicePage::createSerialGroup()
{
    auto *group = new QGroupBox(QStringLiteral("串口设置"), contentHost_);
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

    connect(refreshSerialButton_, &QPushButton::clicked, this, &DevicePage::refreshSerialPorts);
    connect(openSerialButton_, &QPushButton::clicked, this, &DevicePage::openSerialPort);
    connect(closeSerialButton_, &QPushButton::clicked, this, &DevicePage::closeSerialPort);
    connect(mockModeCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        if (deviceManager_) {
            deviceManager_->setMotionMockMode(checked);
        }
        if (checked) {
            if (motionController_ && motionController_->isOpen()) {
                motionController_->closePort();
            }
            updateSerialButtons(false);
            if (stateSetter_) {
                stateSetter_(QStringLiteral("模拟模式"));
            }
            appendLog(QStringLiteral("已切换到模拟模式，运动命令不会发送到真实串口。"));
        } else {
            updateSerialButtons(motionController_ && motionController_->isOpen());
            if (stateSetter_) {
                stateSetter_(motionController_ && motionController_->isOpen()
                                 ? QStringLiteral("串口已连接")
                                 : QStringLiteral("真实串口模式"));
            }
            appendLog(QStringLiteral("已切换到真实串口模式，请先刷新并打开串口。"));
        }
    });

    return group;
}

QGroupBox *DevicePage::createMotionControlGroup()
{
    auto *group = new QGroupBox(QStringLiteral("运动控制"), contentHost_);
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

QGroupBox *DevicePage::createMotionCommandGroup()
{
    auto *group = new QGroupBox(QStringLiteral("运动命令"), contentHost_);
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

    connect(resetButton, &QPushButton::clicked, this, &DevicePage::resetPosition);
    connect(queryButton, &QPushButton::clicked, this, &DevicePage::queryPosition);
    connect(versionButton, &QPushButton::clicked, this, &DevicePage::readVersion);
    connect(helpButton, &QPushButton::clicked, this, &DevicePage::readHelp);
    connect(executeButton, &QPushButton::clicked, this, &DevicePage::executeAbsoluteMove);

    return group;
}

QGroupBox *DevicePage::createInstrumentGroup()
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
            } else if (auto *unitLabel = qobject_cast<QLabel *>(unitWidget)) {
                unit = unitLabel->text();
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
    connect(analyzerConnectButton_, &QPushButton::clicked, this, &DevicePage::connectSpectrumAnalyzer);
    connect(analyzerDisconnectButton_, &QPushButton::clicked, this, &DevicePage::disconnectSpectrumAnalyzer);
    connect(queryIdnButton_, &QPushButton::clicked, this, &DevicePage::querySpectrumIdn);
    connect(applyAnalyzerConfigButton_, &QPushButton::clicked, this, &DevicePage::applySpectrumConfig);
    connect(singleSweepButton_, &QPushButton::clicked, this, &DevicePage::runSingleSpectrumSweep);
    connect(analyzerTypeCombo_, &QComboBox::currentTextChanged, this, &DevicePage::updateAnalyzerMethodHint);
    updateAnalyzerMethodHint(analyzerTypeCombo_->currentText());
    updateAnalyzerButtons(false);
    return group;
}

void DevicePage::appendLog(const QString &text)
{
    if (logEdit_) {
        logEdit_->appendPlainText(QStringLiteral("[%1] %2")
                                    .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), text));
        logEdit_->verticalScrollBar()->setValue(logEdit_->verticalScrollBar()->maximum());
    }
    emit logMessage(text);
}

void DevicePage::notifyPositionChanged()
{
    if (positionChangedHandler_) {
        positionChangedHandler_(currentX_, currentY_, currentZ_);
    }
}

void DevicePage::openSerialPort()
{
    if (isMockMode()) {
        updateSerialButtons(true);
        if (stateSetter_) {
            stateSetter_(QStringLiteral("串口已连接"));
        }
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

void DevicePage::closeSerialPort()
{
    if (isMockMode()) {
        updateSerialButtons(false);
        if (stateSetter_) {
            stateSetter_(QStringLiteral("串口已关闭"));
        }
        appendLog(QStringLiteral("模拟串口已关闭。"));
        return;
    }

    if (motionController_) {
        motionController_->closePort();
    }
}

void DevicePage::jogAxis(const QString &axis, double direction)
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

    notifyPositionChanged();
    appendLog(QStringLiteral("模拟模式：点动 %1%2 %3 mm，当前位置 %4")
                  .arg(axis,
                       direction > 0.0 ? QStringLiteral("+") : QStringLiteral("-"),
                       mmText(jogStep_),
                       positionText(currentX_, currentY_, currentZ_)));
}

bool DevicePage::ensureRealMotionReady()
{
    if (isMockMode()) {
        return false;
    }

    if (!motionController_ || !motionController_->isOpen()) {
        appendLog(QStringLiteral("请先打开串口。"));
        if (stateSetter_) {
            stateSetter_(QStringLiteral("串口未连接"));
        }
        return false;
    }

    return true;
}

bool DevicePage::validateMotionTarget(double x, double y, double z)
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

void DevicePage::resetPosition()
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
    notifyPositionChanged();
    appendLog(QStringLiteral("模拟模式：执行复位，当前位置已清零。"));
}

void DevicePage::queryPosition()
{
    if (!isMockMode()) {
        if (ensureRealMotionReady()) {
            motionController_->queryPosition();
        }
        return;
    }

    appendLog(QStringLiteral("模拟模式：当前位置 %1").arg(positionText(currentX_, currentY_, currentZ_)));
}

void DevicePage::readVersion()
{
    if (!isMockMode()) {
        if (ensureRealMotionReady()) {
            motionController_->readVersion();
        }
        return;
    }

    appendLog(QStringLiteral("模拟模式：Mock GRBL Controller v1.0"));
}

void DevicePage::readHelp()
{
    if (!isMockMode()) {
        if (ensureRealMotionReady()) {
            motionController_->readHelp();
        }
        return;
    }

    appendLog(QStringLiteral("模拟模式：支持命令 $H、?、$I、G1X..Y..Z..F.."));
}

void DevicePage::executeAbsoluteMove()
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
    notifyPositionChanged();
    appendLog(QStringLiteral("模拟模式：执行 G1 X%1 Y%2 Z%3 F%4，当前位置 %5")
                  .arg(mmText(currentX_),
                       mmText(currentY_),
                       mmText(currentZ_),
                       QString::number(feed, 'f', 0),
                       positionText(currentX_, currentY_, currentZ_)));
}

void DevicePage::updateSerialButtons(bool connected)
{
    if (openSerialButton_) {
        openSerialButton_->setEnabled(!connected);
    }
    if (closeSerialButton_) {
        closeSerialButton_->setEnabled(connected);
    }
}

void DevicePage::connectSpectrumAnalyzer()
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
        QMessageBox::warning(messageBoxParent_, QStringLiteral("频谱仪连接失败"), message);
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
    emit analyzerConnectedChanged(true);
}

void DevicePage::disconnectSpectrumAnalyzer()
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
    emit analyzerConnectedChanged(false);
}

void DevicePage::querySpectrumIdn()
{
    if (!deviceManager_ || !currentAnalyzer_ || !currentAnalyzer_->isConnected()) {
        const QString message = QStringLiteral("请先连接频谱仪。");
        appendLog(message);
        QMessageBox::warning(messageBoxParent_, QStringLiteral("频谱仪未连接"), message);
        return;
    }

    const QString idn = deviceManager_->querySpectrumIdn();
    if (idn.isEmpty()) {
        const QString message = deviceManager_->lastError().isEmpty()
            ? QStringLiteral("IDN 查询无返回。")
            : deviceManager_->lastError();
        appendLog(QStringLiteral("IDN 查询失败：%1").arg(message));
        QMessageBox::warning(messageBoxParent_, QStringLiteral("IDN 查询失败"), message);
        return;
    }

    appendLog(QStringLiteral("频谱仪 IDN：%1").arg(idn));
}

void DevicePage::applySpectrumConfig()
{
    if (!deviceManager_ || !currentAnalyzer_ || !currentAnalyzer_->isConnected()) {
        const QString message = QStringLiteral("请先连接仪表。");
        appendLog(message);
        QMessageBox::warning(messageBoxParent_, QStringLiteral("频谱仪未连接"), message);
        return;
    }

    currentSpectrumConfig_ = readSpectrumConfig();
    if (!deviceManager_->configureSpectrum(currentSpectrumConfig_)) {
        const QString message = deviceManager_->lastError().isEmpty()
            ? QStringLiteral("应用仪表配置失败。")
            : deviceManager_->lastError();
        appendLog(QStringLiteral("应用仪表配置失败：%1").arg(message));
        QMessageBox::warning(messageBoxParent_, QStringLiteral("配置失败"), message);
        return;
    }

    appendLog(QStringLiteral("应用仪表配置成功：%1 Hz ~ %2 Hz，RBW=%3 Hz，点数=%4")
                  .arg(currentSpectrumConfig_.startFreqHz, 0, 'f', 0)
                  .arg(currentSpectrumConfig_.stopFreqHz, 0, 'f', 0)
                  .arg(currentSpectrumConfig_.rbwHz, 0, 'f', 0)
                  .arg(currentSpectrumConfig_.sweepPoints));
}

void DevicePage::runSingleSpectrumSweep()
{
    if (!currentAnalyzer_ || !currentAnalyzer_->isConnected()) {
        const QString message = QStringLiteral("请先连接仪表。");
        appendLog(message);
        QMessageBox::warning(messageBoxParent_, QStringLiteral("频谱仪未连接"), message);
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
        QMessageBox::warning(messageBoxParent_, QStringLiteral("单次扫描失败"), message);
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

void DevicePage::updateAnalyzerButtons(bool connected)
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

void DevicePage::updateAnalyzerMethodHint(const QString &analyzerName)
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

double DevicePage::readFrequencyWithUnit(QLineEdit *edit, QComboBox *unitCombo) const
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

} // namespace NFSScanner::UI
