#include "ui/DeviceStatusBar.h"

#include "app/AppVersion.h"

#include <QHBoxLayout>
#include <QLabel>

namespace NFSScanner::UI {

namespace {

QLabel *makeChip(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("deviceStatusChip"));
    label->setMargin(6);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

} // namespace

DeviceStatusBar::DeviceStatusBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("deviceStatusBar"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(8);

    motionChip_ = makeChip(QStringLiteral("运动：—"), this);
    spectrumChip_ = makeChip(QStringLiteral("频谱：—"), this);
    cameraChip_ = makeChip(QStringLiteral("相机：—"), this);
    systemChip_ = makeChip(QStringLiteral("系统：—"), this);

    layout->addWidget(motionChip_);
    layout->addWidget(spectrumChip_);
    layout->addWidget(cameraChip_);
    layout->addWidget(systemChip_);
    layout->addStretch(1);

    refresh();
}

void DeviceStatusBar::bindDeviceManager(NFSScanner::Core::DeviceManager *deviceManager)
{
    if (deviceManager_ == deviceManager) {
        return;
    }

    if (deviceManager_) {
        disconnect(deviceManager_, nullptr, this, nullptr);
    }

    deviceManager_ = deviceManager;
    if (!deviceManager_) {
        refresh();
        return;
    }

    connect(deviceManager_, &NFSScanner::Core::DeviceManager::deviceStateChanged,
            this, &DeviceStatusBar::refresh);
    refresh();
}

void DeviceStatusBar::refresh()
{
    if (!deviceManager_) {
        applyChipStyle(motionChip_, NFSScanner::Core::DeviceConnectionState::Disconnected);
        applyChipStyle(spectrumChip_, NFSScanner::Core::DeviceConnectionState::Disconnected);
        applyChipStyle(cameraChip_, NFSScanner::Core::DeviceConnectionState::Disconnected);
        motionChip_->setText(QStringLiteral("运动：—"));
        spectrumChip_->setText(QStringLiteral("频谱：—"));
        cameraChip_->setText(QStringLiteral("相机：—"));
        systemChip_->setText(QStringLiteral("系统：v%1").arg(QStringLiteral(APP_VERSION)));
        return;
    }

    const auto motionState = deviceManager_->motionState();
    const auto spectrumState = deviceManager_->spectrumState();
    const auto cameraState = deviceManager_->cameraState();

    motionChip_->setText(QStringLiteral("运动：%1")
                             .arg(NFSScanner::Core::deviceConnectionStateText(motionState)));
    spectrumChip_->setText(QStringLiteral("频谱：%1")
                               .arg(NFSScanner::Core::deviceConnectionStateText(spectrumState)));
    cameraChip_->setText(QStringLiteral("相机：%1")
                             .arg(NFSScanner::Core::deviceConnectionStateText(cameraState)));
    systemChip_->setText(QStringLiteral("系统：v%1").arg(QStringLiteral(APP_VERSION)));

    applyChipStyle(motionChip_, motionState);
    applyChipStyle(spectrumChip_, spectrumState);
    applyChipStyle(cameraChip_, cameraState);
    applyChipStyle(systemChip_, NFSScanner::Core::DeviceConnectionState::Connected);
}

void DeviceStatusBar::applyChipStyle(QLabel *label, NFSScanner::Core::DeviceConnectionState state) const
{
    if (!label) {
        return;
    }

    QString bg = QStringLiteral("#334155");
    QString fg = QStringLiteral("#e2e8f0");
    switch (state) {
    case NFSScanner::Core::DeviceConnectionState::Connected:
        bg = QStringLiteral("#166534");
        fg = QStringLiteral("#ecfdf5");
        break;
    case NFSScanner::Core::DeviceConnectionState::Mock:
        bg = QStringLiteral("#1d4ed8");
        fg = QStringLiteral("#eff6ff");
        break;
    case NFSScanner::Core::DeviceConnectionState::Fault:
        bg = QStringLiteral("#991b1b");
        fg = QStringLiteral("#fef2f2");
        break;
    case NFSScanner::Core::DeviceConnectionState::Disconnected:
    default:
        break;
    }

    label->setStyleSheet(QStringLiteral("QLabel { background:%1; color:%2; border-radius:6px; padding:4px 10px; }")
                             .arg(bg, fg));
}

} // namespace NFSScanner::UI
