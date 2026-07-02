#pragma once

#include <QDialog>

class QComboBox;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace NFSScanner::Core {
class DeviceManager;
class ScanConfig;
}

namespace NFSScanner::License {
class LicenseManager;
}

namespace NFSScanner::Project {
class ProjectManager;
}

namespace NFSScanner::UI {

class HardwareBringupWizard final : public QDialog
{
    Q_OBJECT

public:
    explicit HardwareBringupWizard(Core::DeviceManager *deviceManager,
                                   License::LicenseManager *licenseManager,
                                   Project::ProjectManager *projectManager,
                                   QWidget *parent = nullptr);

private:
    void appendLog(const QString &text);
    void refreshStepList();
    void runCurrentStep();
    void runAllSteps();
    void exportReport();

    Core::DeviceManager *deviceManager_ = nullptr;
    License::LicenseManager *licenseManager_ = nullptr;
    Project::ProjectManager *projectManager_ = nullptr;

    QComboBox *profileCombo_ = nullptr;
    QListWidget *stepList_ = nullptr;
    QPlainTextEdit *logEdit_ = nullptr;
    QPushButton *runStepButton_ = nullptr;
    QPushButton *skipStepButton_ = nullptr;
    QPushButton *retryStepButton_ = nullptr;
    QPushButton *runAllButton_ = nullptr;
    QPushButton *exportButton_ = nullptr;

    int currentStepIndex_ = 0;
    class BringupRunResultHolder;
    BringupRunResultHolder *resultHolder_ = nullptr;
};

void showHardwareBringupWizard(Core::DeviceManager *deviceManager,
                               License::LicenseManager *licenseManager,
                               Project::ProjectManager *projectManager,
                               QWidget *parent);

} // namespace NFSScanner::UI
