#include "ui/HardwareBringupWizard.h"

#include "core/HardwareBringupPlan.h"
#include "core/HardwareBringupRunner.h"
#include "core/DeviceManager.h"
#include "license/LicenseManager.h"
#include "project/ProjectManager.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace NFSScanner::UI {

class HardwareBringupWizard::BringupRunResultHolder
{
public:
    Core::HardwareBringupRunResult result;
    Core::HardwareBringupPlan plan;
};

HardwareBringupWizard::HardwareBringupWizard(Core::DeviceManager *deviceManager,
                                             License::LicenseManager *licenseManager,
                                             Project::ProjectManager *projectManager,
                                             QWidget *parent)
    : QDialog(parent)
    , deviceManager_(deviceManager)
    , licenseManager_(licenseManager)
    , projectManager_(projectManager)
    , resultHolder_(new BringupRunResultHolder)
{
    setWindowTitle(QStringLiteral("硬件接入向导"));
    resize(860, 620);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QStringLiteral("选择 Profile 并按步骤执行 bring-up 测试。"), this));

    profileCombo_ = new QComboBox(this);
    profileCombo_->addItems(Core::HardwareBringupPlan::availableProfiles());
    profileCombo_->setCurrentText(QStringLiteral("mock_all"));
    layout->addWidget(profileCombo_);

    stepList_ = new QListWidget(this);
    layout->addWidget(stepList_, 1);

    logEdit_ = new QPlainTextEdit(this);
    logEdit_->setReadOnly(true);
    logEdit_->setMaximumBlockCount(2000);
    layout->addWidget(logEdit_, 1);

    auto *buttonRow = new QHBoxLayout;
    runStepButton_ = new QPushButton(QStringLiteral("运行当前步骤"), this);
    skipStepButton_ = new QPushButton(QStringLiteral("跳过并记录"), this);
    retryStepButton_ = new QPushButton(QStringLiteral("重试"), this);
    runAllButton_ = new QPushButton(QStringLiteral("运行全部"), this);
    exportButton_ = new QPushButton(QStringLiteral("导出报告"), this);
    buttonRow->addWidget(runStepButton_);
    buttonRow->addWidget(skipStepButton_);
    buttonRow->addWidget(retryStepButton_);
    buttonRow->addWidget(runAllButton_);
    buttonRow->addWidget(exportButton_);
    layout->addLayout(buttonRow);

    connect(profileCombo_, &QComboBox::currentTextChanged, this, [this](const QString &profile) {
        if (deviceManager_) {
            deviceManager_->loadHardwareProfile(profile);
        }
        resultHolder_->plan = Core::HardwareBringupPlan::planForProfile(profile);
        currentStepIndex_ = 0;
        refreshStepList();
    });
    connect(runStepButton_, &QPushButton::clicked, this, &HardwareBringupWizard::runCurrentStep);
    connect(skipStepButton_, &QPushButton::clicked, this, [this]() {
        if (currentStepIndex_ >= resultHolder_->plan.steps.size()) {
            return;
        }
        Core::BringupStepResult skipped;
        const auto &def = resultHolder_->plan.steps.at(currentStepIndex_);
        skipped.id = def.id;
        skipped.title = def.title;
        skipped.status = Core::BringupStepStatus::Skipped;
        skipped.message = QStringLiteral("用户跳过。");
        resultHolder_->result.steps.append(skipped);
        appendLog(QStringLiteral("[SKIP] %1").arg(def.title));
        ++currentStepIndex_;
        refreshStepList();
    });
    connect(retryStepButton_, &QPushButton::clicked, this, &HardwareBringupWizard::runCurrentStep);
    connect(runAllButton_, &QPushButton::clicked, this, &HardwareBringupWizard::runAllSteps);
    connect(exportButton_, &QPushButton::clicked, this, &HardwareBringupWizard::exportReport);

    resultHolder_->plan = Core::HardwareBringupPlan::planForProfile(profileCombo_->currentText());
    refreshStepList();
}

void HardwareBringupWizard::appendLog(const QString &text)
{
    if (logEdit_) {
        logEdit_->appendPlainText(text);
    }
}

void HardwareBringupWizard::refreshStepList()
{
    if (!stepList_) {
        return;
    }
    stepList_->clear();
    for (int i = 0; i < resultHolder_->plan.steps.size(); ++i) {
        const auto &step = resultHolder_->plan.steps.at(i);
        QString prefix = i == currentStepIndex_ ? QStringLiteral(">> ") : QStringLiteral("   ");
        stepList_->addItem(prefix + step.title);
    }
}

void HardwareBringupWizard::runCurrentStep()
{
    if (!deviceManager_ || currentStepIndex_ >= resultHolder_->plan.steps.size()) {
        return;
    }
    const bool projectExists = projectManager_ && projectManager_->hasOpenProject();
    const bool licenseValid = !licenseManager_
        || licenseManager_->status() == License::LicenseStatus::Valid
        || licenseManager_->status() == License::LicenseStatus::Demo;
    const auto stepResult = Core::HardwareBringupRunner::runStep(deviceManager_,
                                                                 resultHolder_->plan.steps.at(currentStepIndex_),
                                                                 nullptr,
                                                                 projectExists,
                                                                 licenseValid);
    resultHolder_->result.steps.append(stepResult);
    appendLog(QStringLiteral("[%1] %2: %3")
                  .arg(Core::bringupStepStatusText(stepResult.status), stepResult.title, stepResult.message));
    ++currentStepIndex_;
    refreshStepList();
}

void HardwareBringupWizard::runAllSteps()
{
    if (!deviceManager_) {
        return;
    }
    const bool projectExists = projectManager_ && projectManager_->hasOpenProject();
    const bool licenseValid = !licenseManager_
        || licenseManager_->status() == License::LicenseStatus::Valid
        || licenseManager_->status() == License::LicenseStatus::Demo;
    if (deviceManager_->loadHardwareProfile(profileCombo_->currentText())) {
        appendLog(QStringLiteral("Profile 已加载: %1").arg(profileCombo_->currentText()));
    }
    resultHolder_->result = Core::HardwareBringupRunner::runPlan(deviceManager_,
                                                                resultHolder_->plan,
                                                                nullptr,
                                                                projectExists,
                                                                licenseValid);
    currentStepIndex_ = resultHolder_->plan.steps.size();
    for (const Core::BringupStepResult &step : resultHolder_->result.steps) {
        appendLog(QStringLiteral("[%1] %2: %3")
                      .arg(Core::bringupStepStatusText(step.status), step.title, step.message));
    }
    refreshStepList();
    appendLog(QStringLiteral("报告: %1").arg(resultHolder_->result.exportPath));
}

void HardwareBringupWizard::exportReport()
{
    QString path;
    if (Core::HardwareBringupRunner::exportReport(resultHolder_->result, &path)) {
        appendLog(QStringLiteral("报告已导出: %1").arg(path));
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

void showHardwareBringupWizard(Core::DeviceManager *deviceManager,
                               License::LicenseManager *licenseManager,
                               Project::ProjectManager *projectManager,
                               QWidget *parent)
{
    HardwareBringupWizard dialog(deviceManager, licenseManager, projectManager, parent);
    dialog.exec();
}

} // namespace NFSScanner::UI
