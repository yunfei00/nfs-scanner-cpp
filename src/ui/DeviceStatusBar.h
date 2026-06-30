#pragma once

#include "core/DeviceManager.h"

#include <QWidget>

class QLabel;

namespace NFSScanner::License {
class LicenseManager;
}

namespace NFSScanner::Project {
class ProjectManager;
}

namespace NFSScanner::Core {
class DeviceManager;
}

namespace NFSScanner::UI {

class DeviceStatusBar final : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceStatusBar(QWidget *parent = nullptr);

    void bindDeviceManager(NFSScanner::Core::DeviceManager *deviceManager);
    void bindProjectManager(NFSScanner::Project::ProjectManager *projectManager);
    void bindLicenseManager(NFSScanner::License::LicenseManager *licenseManager);
    void setScanStateText(const QString &text);
    void refresh();

private:
    void applyChipStyle(QLabel *label, NFSScanner::Core::DeviceConnectionState state) const;

    NFSScanner::Core::DeviceManager *deviceManager_ = nullptr;
    NFSScanner::Project::ProjectManager *projectManager_ = nullptr;
    NFSScanner::License::LicenseManager *licenseManager_ = nullptr;
    QString scanStateText_ = QStringLiteral("空闲");

    QLabel *motionChip_ = nullptr;
    QLabel *spectrumChip_ = nullptr;
    QLabel *cameraChip_ = nullptr;
    QLabel *projectChip_ = nullptr;
    QLabel *licenseChip_ = nullptr;
    QLabel *scanChip_ = nullptr;
};

} // namespace NFSScanner::UI
