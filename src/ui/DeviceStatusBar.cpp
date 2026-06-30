#include "ui/DeviceStatusBar.h"

#include "app/AppVersion.h"
#include "license/LicenseManager.h"
#include "project/ProjectManager.h"

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
    layout->setSpacing(6);

    motionChip_ = makeChip(QStringLiteral("运动：—"), this);
    spectrumChip_ = makeChip(QStringLiteral("频谱：—"), this);
    cameraChip_ = makeChip(QStringLiteral("相机：—"), this);
    projectChip_ = makeChip(QStringLiteral("项目：—"), this);
    licenseChip_ = makeChip(QStringLiteral("授权：—"), this);
    scanChip_ = makeChip(QStringLiteral("扫描：空闲"), this);

    layout->addWidget(motionChip_);
    layout->addWidget(spectrumChip_);
    layout->addWidget(cameraChip_);
    layout->addWidget(projectChip_);
    layout->addWidget(licenseChip_);
    layout->addWidget(scanChip_);
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
    if (deviceManager_) {
        connect(deviceManager_, &NFSScanner::Core::DeviceManager::deviceStateChanged,
                this, &DeviceStatusBar::refresh);
    }
    refresh();
}

void DeviceStatusBar::bindProjectManager(NFSScanner::Project::ProjectManager *projectManager)
{
    if (projectManager_ == projectManager) {
        return;
    }
    if (projectManager_) {
        disconnect(projectManager_, nullptr, this, nullptr);
    }
    projectManager_ = projectManager;
    if (projectManager_) {
        connect(projectManager_, &NFSScanner::Project::ProjectManager::projectChanged,
                this, &DeviceStatusBar::refresh);
    }
    refresh();
}

void DeviceStatusBar::bindLicenseManager(NFSScanner::License::LicenseManager *licenseManager)
{
    licenseManager_ = licenseManager;
    refresh();
}

void DeviceStatusBar::setScanStateText(const QString &text)
{
    scanStateText_ = text.isEmpty() ? QStringLiteral("空闲") : text;
    refresh();
}

void DeviceStatusBar::refresh()
{
    if (deviceManager_) {
        motionChip_->setText(QStringLiteral("运动：%1")
                                 .arg(NFSScanner::Core::deviceConnectionStateText(deviceManager_->motionState())));
        spectrumChip_->setText(QStringLiteral("频谱：%1")
                                   .arg(NFSScanner::Core::deviceConnectionStateText(deviceManager_->spectrumState())));
        cameraChip_->setText(QStringLiteral("相机：%1")
                                 .arg(NFSScanner::Core::deviceConnectionStateText(deviceManager_->cameraState())));
        applyChipStyle(motionChip_, deviceManager_->motionState());
        applyChipStyle(spectrumChip_, deviceManager_->spectrumState());
        applyChipStyle(cameraChip_, deviceManager_->cameraState());
    } else {
        motionChip_->setText(QStringLiteral("运动：—"));
        spectrumChip_->setText(QStringLiteral("频谱：—"));
        cameraChip_->setText(QStringLiteral("相机：—"));
    }

    if (projectManager_ && projectManager_->hasOpenProject()) {
        projectChip_->setText(QStringLiteral("项目：%1").arg(projectManager_->currentProject().name));
        applyChipStyle(projectChip_, NFSScanner::Core::DeviceConnectionState::Connected);
    } else {
        projectChip_->setText(QStringLiteral("项目：无"));
        applyChipStyle(projectChip_, NFSScanner::Core::DeviceConnectionState::Disconnected);
    }

    if (licenseManager_) {
        licenseChip_->setText(QStringLiteral("授权：%1")
                                  .arg(License::licenseStatusText(licenseManager_->status())));
        applyChipStyle(licenseChip_, licenseManager_->status() == License::LicenseStatus::Valid
                                         ? NFSScanner::Core::DeviceConnectionState::Connected
                                         : NFSScanner::Core::DeviceConnectionState::Mock);
    } else {
        licenseChip_->setText(QStringLiteral("授权：Demo"));
        applyChipStyle(licenseChip_, NFSScanner::Core::DeviceConnectionState::Mock);
    }

    scanChip_->setText(QStringLiteral("扫描：%1").arg(scanStateText_));
    applyChipStyle(scanChip_, scanStateText_.contains(QStringLiteral("运行"))
                                ? NFSScanner::Core::DeviceConnectionState::Connected
                                : NFSScanner::Core::DeviceConnectionState::Disconnected);
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
    default:
        break;
    }

    label->setStyleSheet(QStringLiteral("QLabel { background:%1; color:%2; border-radius:6px; padding:4px 8px; font-size:11px; }")
                             .arg(bg, fg));
}

} // namespace NFSScanner::UI
