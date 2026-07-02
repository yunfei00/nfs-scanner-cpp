#include "ui/HardwareDebugDialog.h"

#include "core/DeviceManager.h"
#include "devices/camera/ICamera.h"
#include "devices/motion/SerialMotionController.h"
#include "devices/probe/IProbeController.h"
#include "devices/spectrum/ScpiCommandLogger.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace NFSScanner::UI {

HardwareDebugDialog::HardwareDebugDialog(Core::DeviceManager *deviceManager,
                                         Devices::Motion::SerialMotionController *motionController,
                                         QWidget *parent)
    : QDialog(parent)
    , deviceManager_(deviceManager)
    , motionController_(motionController)
{
    setWindowTitle(QStringLiteral("硬件调试面板"));
    resize(720, 520);

    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildMotionTab(), QStringLiteral("Motion"));
    tabs->addTab(buildSpectrumTab(), QStringLiteral("Spectrum"));
    tabs->addTab(buildCameraTab(), QStringLiteral("Camera"));
    tabs->addTab(buildProbeTab(), QStringLiteral("Probe"));
    layout->addWidget(tabs);

    logEdit_ = new QPlainTextEdit(this);
    logEdit_->setReadOnly(true);
    logEdit_->setMaximumBlockCount(2000);
    layout->addWidget(new QLabel(QStringLiteral("调试日志"), this));
    layout->addWidget(logEdit_, 1);
}

void HardwareDebugDialog::appendLog(const QString &text)
{
    if (logEdit_) {
        logEdit_->appendPlainText(text);
    }
}

QWidget *HardwareDebugDialog::buildMotionTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QVBoxLayout(widget);

    auto *warning = new QLabel(QStringLiteral("危险运动命令需确认后发送。默认折叠原始命令区。"), widget);
    layout->addWidget(warning);

    auto *rawGroup = new QGroupBox(QStringLiteral("原始 GRBL 命令（谨慎）"), widget);
    auto *rawLayout = new QHBoxLayout(rawGroup);
    auto *cmdEdit = new QLineEdit(rawGroup);
    cmdEdit->setPlaceholderText(QStringLiteral("$I / ? / $X / G1X1F100"));
    auto *sendButton = new QPushButton(QStringLiteral("发送"), rawGroup);
    rawLayout->addWidget(cmdEdit, 1);
    rawLayout->addWidget(sendButton);
    rawGroup->setCheckable(true);
    rawGroup->setChecked(false);
    layout->addWidget(rawGroup);

    auto *row = new QHBoxLayout;
    auto *queryBtn = new QPushButton(QStringLiteral("查询状态 ?"), widget);
    auto *homeBtn = new QPushButton(QStringLiteral("Home $H"), widget);
    auto *stopBtn = new QPushButton(QStringLiteral("Feed Hold !"), widget);
    row->addWidget(queryBtn);
    row->addWidget(homeBtn);
    row->addWidget(stopBtn);
    layout->addLayout(row);

    connect(sendButton, &QPushButton::clicked, this, [this, cmdEdit, rawGroup]() {
        if (!rawGroup->isChecked()) {
            QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先展开原始命令区。"));
            return;
        }
        if (!motionController_) {
            return;
        }
        const QString cmd = cmdEdit->text().trimmed();
        if (cmd.contains(QStringLiteral("$H"), Qt::CaseInsensitive)
            || cmd.startsWith(QStringLiteral("G1"), Qt::CaseInsensitive)) {
            const auto answer = QMessageBox::question(this,
                                                      QStringLiteral("确认运动命令"),
                                                      QStringLiteral("确认发送：%1 ?").arg(cmd));
            if (answer != QMessageBox::Yes) {
                return;
            }
        }
        if (motionController_->sendRawCommand(cmd)) {
            appendLog(QStringLiteral("TX %1").arg(cmd));
        }
    });
    connect(queryBtn, &QPushButton::clicked, this, [this]() {
        if (motionController_) {
            motionController_->queryPosition();
            appendLog(QStringLiteral("已发送 ?"));
        }
    });
    connect(homeBtn, &QPushButton::clicked, this, [this]() {
        if (!motionController_) {
            return;
        }
        if (QMessageBox::question(this, QStringLiteral("确认 Home"), QStringLiteral("确认执行 $H ?"))
            == QMessageBox::Yes) {
            motionController_->home();
            appendLog(QStringLiteral("已发送 $H"));
        }
    });
    connect(stopBtn, &QPushButton::clicked, this, [this]() {
        if (motionController_) {
            motionController_->feedHold();
            appendLog(QStringLiteral("已发送 Feed Hold"));
        }
    });

    layout->addStretch(1);
    return widget;
}

QWidget *HardwareDebugDialog::buildSpectrumTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QVBoxLayout(widget);
    auto *idnBtn = new QPushButton(QStringLiteral("查询 IDN"), widget);
    auto *errBtn = new QPushButton(QStringLiteral("查询最近错误"), widget);
    auto *clearBtn = new QPushButton(QStringLiteral("清空最近错误"), widget);
    layout->addWidget(idnBtn);
    layout->addWidget(errBtn);
    layout->addWidget(clearBtn);

    connect(idnBtn, &QPushButton::clicked, this, [this]() {
        if (!deviceManager_) {
            return;
        }
        const QString idn = deviceManager_->querySpectrumIdn();
        appendLog(QStringLiteral("IDN: %1").arg(idn));
    });
    connect(errBtn, &QPushButton::clicked, this, [this]() {
        appendLog(QStringLiteral("SCPI Error: %1").arg(Devices::Spectrum::ScpiCommandLogger::lastErrorMessage()));
    });
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        Devices::Spectrum::ScpiCommandLogger::clearRecentErrors();
        appendLog(QStringLiteral("已清空 SCPI 最近错误。"));
    });
    layout->addStretch(1);
    return widget;
}

QWidget *HardwareDebugDialog::buildCameraTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QVBoxLayout(widget);
    auto *captureBtn = new QPushButton(QStringLiteral("拍照"), widget);
    layout->addWidget(captureBtn);
    connect(captureBtn, &QPushButton::clicked, this, [this]() {
        if (!deviceManager_ || !deviceManager_->camera()) {
            appendLog(QStringLiteral("相机未连接。"));
            return;
        }
        const QImage img = deviceManager_->camera()->captureFrame();
        appendLog(img.isNull() ? QStringLiteral("拍照失败。") : QStringLiteral("拍照成功 %1x%2").arg(img.width()).arg(img.height()));
    });
    layout->addStretch(1);
    return widget;
}

QWidget *HardwareDebugDialog::buildProbeTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QHBoxLayout(widget);
    auto *hxBtn = new QPushButton(QStringLiteral("Hx"), widget);
    auto *hyBtn = new QPushButton(QStringLiteral("Hy"), widget);
    layout->addWidget(hxBtn);
    layout->addWidget(hyBtn);
    connect(hxBtn, &QPushButton::clicked, this, [this]() {
        if (deviceManager_ && deviceManager_->probeController()) {
            deviceManager_->probeController()->setOrientation(Devices::Probe::ProbeOrientation::Hx);
            appendLog(QStringLiteral("Probe -> Hx"));
        }
    });
    connect(hyBtn, &QPushButton::clicked, this, [this]() {
        if (deviceManager_ && deviceManager_->probeController()) {
            deviceManager_->probeController()->setOrientation(Devices::Probe::ProbeOrientation::Hy);
            appendLog(QStringLiteral("Probe -> Hy"));
        }
    });
    return widget;
}

} // namespace NFSScanner::UI
