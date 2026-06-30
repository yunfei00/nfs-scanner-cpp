#pragma once

#include <QWidget>

class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

namespace NFSScanner::Core {
class DeviceManager;
}

namespace NFSScanner::License {
class LicenseManager;
}

namespace NFSScanner::Project {
class ProjectManager;
}

namespace NFSScanner::UI {

class AnalysisPage;
class DevicePage;
class ScanPage;

class ReportPage final : public QWidget
{
    Q_OBJECT

public:
    explicit ReportPage(QWidget *parent = nullptr);

    QListWidget *reportList() const;
    QPlainTextEdit *previewEditor() const;
    QWidget *paramPanel() const;

    void bind(ScanPage *scanPage,
              AnalysisPage *analysisPage,
              DevicePage *devicePage,
              NFSScanner::Project::ProjectManager *projectManager,
              NFSScanner::Core::DeviceManager *deviceManager,
              NFSScanner::License::LicenseManager *licenseManager,
              QWidget *messageBoxParent);

    void exportReport(const QString &format);
    void refreshTaskList();

signals:
    void logMessage(const QString &text);

private:
    QWidget *buildParamPanel();
    QString defaultReportsDir() const;

    ScanPage *scanPage_ = nullptr;
    AnalysisPage *analysisPage_ = nullptr;
    DevicePage *devicePage_ = nullptr;
    NFSScanner::Project::ProjectManager *projectManager_ = nullptr;
    NFSScanner::Core::DeviceManager *deviceManager_ = nullptr;
    NFSScanner::License::LicenseManager *licenseManager_ = nullptr;
    QWidget *messageBoxParent_ = nullptr;

    QWidget *paramPanel_ = nullptr;
    QListWidget *reportList_ = nullptr;
    QPlainTextEdit *previewEditor_ = nullptr;
    QLineEdit *operatorEdit_ = nullptr;
    QLineEdit *notesEdit_ = nullptr;
};

} // namespace NFSScanner::UI
