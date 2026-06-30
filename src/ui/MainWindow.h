#pragma once

#include "core/AlignmentManager.h"
#include "core/ScanConfig.h"

#include <QMainWindow>
#include <QString>

class QDockWidget;
class QGroupBox;
class QLabel;
class QPlainTextEdit;
class QListWidget;
class QStackedWidget;
class QStatusBar;
class QTimer;
class QToolBar;
class QWidget;

namespace NFSScanner::Core {
class DeviceManager;
class ScanManager;
}

namespace NFSScanner::Project {
class ProjectManager;
}

namespace NFSScanner::License {
class LicenseManager;
}

namespace NFSScanner::Devices::Motion {
class SerialMotionController;
}

namespace NFSScanner::UI {

class HeatmapView;
class ScanPage;
class DevicePage;
class AnalysisPage;
class ReportPage;
class DeviceStatusBar;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    enum class AppPage {
        Scan = 0,
        Device = 1,
        Analysis = 2,
        Report = 3
    };

private:
    void setupUi();
    void setupMenus();
    void setupToolBar();
    void setupNavigation();
    void setupParamDock();
    void setupAuxiliaryDocks();
    void setupPages();
    void setupScanManager();
    void setupScanPageBindings();
    void setupDevicePageBindings();
    void setupAnalysisPageBindings();
    void setupReportPageBindings();
    void switchToPage(AppPage page);
    void exportCurrentReport(const QString &format = QStringLiteral("html"));
    void showAboutDialog();
    void showDiagnosticsDialog();
    void updateProjectStatusDisplay();

    QGroupBox *createLogGroup();
    void setupStatusBar();

    void appendLog(const QString &text);
    void updateStatusBar();
    void setAppState(const QString &state);

    void startScan();
    void pauseScan();
    void stopScan();

    double currentX_ = 0.0;
    double currentY_ = 0.0;
    double currentZ_ = 0.0;
    QString appState_ = QStringLiteral("就绪");
    QString remainingText_ = QStringLiteral("--");
    QString estimatedFinishText_ = QStringLiteral("--");

    QTimer *clockTimer_ = nullptr;
    NFSScanner::Core::DeviceManager *deviceManager_ = nullptr;
    NFSScanner::Project::ProjectManager *projectManager_ = nullptr;
    NFSScanner::License::LicenseManager *licenseManager_ = nullptr;
    NFSScanner::Core::ScanManager *scanManager_ = nullptr;
    NFSScanner::Devices::Motion::SerialMotionController *motionController_ = nullptr;

    QPlainTextEdit *logEdit_ = nullptr;
    QStatusBar *statusBar_ = nullptr;
    QLabel *statusTextLabel_ = nullptr;
    DeviceStatusBar *deviceStatusBar_ = nullptr;
    HeatmapView *heatmapView_ = nullptr;
    NFSScanner::Core::AlignmentManager alignmentManager_;

    ScanPage *scanPage_ = nullptr;
    DevicePage *devicePage_ = nullptr;
    AnalysisPage *analysisPage_ = nullptr;
    ReportPage *reportPage_ = nullptr;

    QListWidget *navList_ = nullptr;
    QStackedWidget *pageStack_ = nullptr;
    QStackedWidget *paramDockStack_ = nullptr;
    QDockWidget *paramDock_ = nullptr;
    QDockWidget *logDock_ = nullptr;
    QDockWidget *spectrumDock_ = nullptr;
    QDockWidget *statisticsDock_ = nullptr;
    QDockWidget *dataTableDock_ = nullptr;
    QToolBar *mainToolBar_ = nullptr;
    AppPage currentPage_ = AppPage::Scan;
};

} // namespace NFSScanner::UI
