#include "ui/pages/ReportPage.h"

#include "core/DeviceManager.h"
#include "license/LicenseManager.h"
#include "project/ProjectManager.h"
#include "core/ScanConfig.h"
#include "report/ReportData.h"
#include "report/ReportGenerator.h"
#include "ui/pages/AnalysisPage.h"
#include "ui/pages/DevicePage.h"
#include "ui/pages/ScanPage.h"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace NFSScanner::UI {

ReportPage::ReportPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("reportPage"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    reportList_ = new QListWidget(this);
    reportList_->setObjectName(QStringLiteral("reportList"));
    reportList_->setFixedWidth(240);

    previewEditor_ = new QPlainTextEdit(this);
    previewEditor_->setObjectName(QStringLiteral("reportPreviewEditor"));
    previewEditor_->setReadOnly(true);
    previewEditor_->setPlainText(QStringLiteral(
        "NFS Scanner 报告预览\n\n"
        "扫描完成后，左侧将列出可导出的任务。\n"
        "在右侧参数面板填写操作员与备注，然后导出 HTML / Markdown / PDF。"));

    layout->addWidget(reportList_);
    layout->addWidget(previewEditor_, 1);

    paramPanel_ = buildParamPanel();

    connect(reportList_, &QListWidget::currentTextChanged, this, [this](const QString &text) {
        if (!previewEditor_ || text.isEmpty()) {
            return;
        }
        previewEditor_->setPlainText(QStringLiteral("报告任务：%1\n\n使用右侧「导出」按钮生成文件。").arg(text));
    });
}

QListWidget *ReportPage::reportList() const
{
    return reportList_;
}

QPlainTextEdit *ReportPage::previewEditor() const
{
    return previewEditor_;
}

QWidget *ReportPage::paramPanel() const
{
    return paramPanel_;
}

void ReportPage::bind(ScanPage *scanPage,
                      AnalysisPage *analysisPage,
                      DevicePage *devicePage,
                      NFSScanner::Project::ProjectManager *projectManager,
                      NFSScanner::Core::DeviceManager *deviceManager,
                      NFSScanner::License::LicenseManager *licenseManager,
                      QWidget *messageBoxParent)
{
    scanPage_ = scanPage;
    analysisPage_ = analysisPage;
    devicePage_ = devicePage;
    projectManager_ = projectManager;
    deviceManager_ = deviceManager;
    licenseManager_ = licenseManager;
    messageBoxParent_ = messageBoxParent;
    refreshTaskList();
}

QWidget *ReportPage::buildParamPanel()
{
    auto *panel = new QWidget;
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);

    auto *infoGroup = new QGroupBox(QStringLiteral("报告信息"), panel);
    auto *form = new QFormLayout(infoGroup);
    operatorEdit_ = new QLineEdit(infoGroup);
    operatorEdit_->setPlaceholderText(QStringLiteral("操作员"));
    notesEdit_ = new QLineEdit(infoGroup);
    notesEdit_->setPlaceholderText(QStringLiteral("备注"));
    form->addRow(QStringLiteral("操作员"), operatorEdit_);
    form->addRow(QStringLiteral("备注"), notesEdit_);
    layout->addWidget(infoGroup);

    auto *exportGroup = new QGroupBox(QStringLiteral("导出"), panel);
    auto *exportLayout = new QVBoxLayout(exportGroup);
    auto *htmlBtn = new QPushButton(QStringLiteral("导出 HTML"), exportGroup);
    auto *mdBtn = new QPushButton(QStringLiteral("导出 Markdown"), exportGroup);
    auto *pdfBtn = new QPushButton(QStringLiteral("导出 PDF"), exportGroup);
    auto *pngBtn = new QPushButton(QStringLiteral("导出 PNG 图片集"), exportGroup);
    exportLayout->addWidget(htmlBtn);
    exportLayout->addWidget(mdBtn);
    exportLayout->addWidget(pdfBtn);
    exportLayout->addWidget(pngBtn);
    layout->addWidget(exportGroup);
    layout->addStretch(1);

    connect(htmlBtn, &QPushButton::clicked, this, [this]() { exportReport(QStringLiteral("html")); });
    connect(mdBtn, &QPushButton::clicked, this, [this]() { exportReport(QStringLiteral("md")); });
    connect(pdfBtn, &QPushButton::clicked, this, [this]() { exportReport(QStringLiteral("pdf")); });
    connect(pngBtn, &QPushButton::clicked, this, [this]() { exportReport(QStringLiteral("png")); });

    return panel;
}

QString ReportPage::defaultReportsDir() const
{
    if (projectManager_ && projectManager_->hasOpenProject()) {
        const QString dir = QDir(projectManager_->currentProject().rootPath).filePath(QStringLiteral("reports"));
        QDir().mkpath(dir);
        return dir;
    }
    if (projectManager_) {
        const QString dir = QDir(projectManager_->workspaceRoot()).filePath(QStringLiteral("reports"));
        QDir().mkpath(dir);
        return dir;
    }
    return QDir::tempPath();
}

void ReportPage::refreshTaskList()
{
    if (!reportList_) {
        return;
    }
    reportList_->clear();
    const QString scanDir = analysisPage_ ? analysisPage_->resultDir() : QString();
    if (!scanDir.isEmpty()) {
        reportList_->addItem(QDir(scanDir).dirName());
    }
    if (reportList_->count() == 0) {
        reportList_->addItem(QStringLiteral("(无扫描任务，完成扫描后刷新)"));
    }
    reportList_->setCurrentRow(0);
}

void ReportPage::exportReport(const QString &format)
{
    if (!messageBoxParent_) {
        return;
    }

    const Core::ScanConfig scanConfig = scanPage_ ? scanPage_->readScanConfigFromUi() : Core::ScanConfig{};
    Report::ReportData data;
    data.projectName = projectManager_ && projectManager_->hasOpenProject()
        ? projectManager_->currentProject().name
        : (scanConfig.projectName.isEmpty() ? QStringLiteral("Demo") : scanConfig.projectName);
    data.scanTaskDir = analysisPage_ ? analysisPage_->resultDir() : QString();
    data.scanTime = QDateTime::currentDateTime();
    data.operatorName = operatorEdit_ ? operatorEdit_->text() : QString();
    data.traceId = analysisPage_ ? analysisPage_->currentTraceId() : QString();
    data.lutName = analysisPage_ ? analysisPage_->currentLutName() : QStringLiteral("turbo");
    data.vmin = analysisPage_ ? analysisPage_->currentVmin() : 0.0;
    data.vmax = analysisPage_ ? analysisPage_->currentVmax() : 1.0;
    data.heatmapImage = analysisPage_ ? analysisPage_->heatmapImage() : QImage();
    data.notes = notesEdit_ && !notesEdit_->text().isEmpty() ? notesEdit_->text() : scanConfig.testName;

    if (devicePage_) {
        const auto spectrumConfig = devicePage_->currentSpectrumConfig();
        if (spectrumConfig.stopFreqHz > 0.0) {
            data.startFrequencyHz = spectrumConfig.startFreqHz;
            data.stopFrequencyHz = spectrumConfig.stopFreqHz;
        }
    }
    data.deviceSummary = deviceManager_
        ? QStringLiteral("运动:%1 频谱:%2 相机:%3")
              .arg(Core::deviceConnectionStateText(deviceManager_->motionState()),
                   Core::deviceConnectionStateText(deviceManager_->spectrumState()),
                   Core::deviceConnectionStateText(deviceManager_->cameraState()))
        : QStringLiteral("Demo");

    Report::ReportGenerator generator;
    const QString reportsDir = defaultReportsDir();

    if (format == QStringLiteral("png")) {
        const QString dir = QFileDialog::getExistingDirectory(messageBoxParent_,
                                                              QStringLiteral("选择 PNG 导出目录"),
                                                              reportsDir);
        if (dir.isEmpty()) {
            return;
        }
        if (!generator.exportPngImages(data, dir)) {
            QMessageBox::warning(messageBoxParent_, QStringLiteral("导出失败"), generator.lastError());
            return;
        }
        emit logMessage(QStringLiteral("PNG 图片集已导出：%1").arg(dir));
        return;
    }

    const QString filter = format == QStringLiteral("pdf")
        ? QStringLiteral("PDF (*.pdf)")
        : (format == QStringLiteral("md") ? QStringLiteral("Markdown (*.md)") : QStringLiteral("HTML (*.html)"));
    const QString defaultName = format == QStringLiteral("pdf")
        ? QStringLiteral("report.pdf")
        : (format == QStringLiteral("md") ? QStringLiteral("report.md") : QStringLiteral("report.html"));
    const QString path = QFileDialog::getSaveFileName(messageBoxParent_,
                                                      QStringLiteral("导出报告"),
                                                      QDir(reportsDir).filePath(defaultName),
                                                      filter);
    if (path.isEmpty()) {
        return;
    }

    bool ok = false;
    if (format == QStringLiteral("pdf")) {
        ok = generator.exportPdf(data, path);
    } else if (format == QStringLiteral("md")) {
        ok = generator.exportMarkdown(data, path);
    } else {
        ok = generator.exportHtml(data, path);
    }
    if (!ok) {
        QMessageBox::warning(messageBoxParent_, QStringLiteral("导出失败"), generator.lastError());
        return;
    }

    emit logMessage(QStringLiteral("报告已导出：%1").arg(path));
    if (previewEditor_) {
        previewEditor_->setPlainText(QStringLiteral("已导出 %1\n项目：%2\nTrace：%3\n路径：%4")
                                         .arg(format.toUpper(), data.projectName, data.traceId, path));
    }
}

} // namespace NFSScanner::UI
