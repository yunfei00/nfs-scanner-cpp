#pragma once

#include "ui/AnalysisController.h"
#include "core/AlignmentManager.h"
#include "core/ScanConfig.h"
#include "core/ScanPoint.h"
#include "devices/spectrum/SpectrumConfig.h"
#include "devices/spectrum/SpectrumTrace.h"

#include <QMainWindow>
#include <QString>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDockWidget;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSlider;
class QStackedWidget;
class QStatusBar;
class QSpinBox;
class QTableWidget;
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

namespace NFSScanner::Devices::Spectrum {
class ISpectrumAnalyzer;
}

namespace NFSScanner::UI {

class HeatmapView;
class ScanPage;
class DevicePage;
class AnalysisPage;
class ReportPage;
class AnalysisController;
class DeviceStatusBar;
class AlignmentEditor;

struct ScanPoint
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

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
    void switchToPage(AppPage page);
    void exportCurrentReport(const QString &format = QStringLiteral("html"));
    void exportAnalysisConfigJson();
    void showAboutDialog();
    void showDiagnosticsDialog();
    void updateProjectStatusDisplay();

    QGroupBox *createSerialGroup();
    QGroupBox *createMotionControlGroup();
    QGroupBox *createMotionCommandGroup();
    QGroupBox *createStepConfigGroup();
    QGroupBox *createTestInfoGroup();
    QGroupBox *createActionGroup();
    QGroupBox *createScanAreaGroup();
    QGroupBox *createInstrumentGroup();
    QGroupBox *createResultGroup();
    QGroupBox *createHeatmapPreviewGroup();
    QGroupBox *createLogGroup();
    void setupStatusBar();
    void setupMotionController();
    void setupScanManager();

    void appendLog(const QString &text);
    void updateStatusBar();
    void setAppState(const QString &state);

    bool isMockMode() const;
    double feedValue() const;
    bool ensureRealMotionReady();
    bool validateMotionTarget(double x, double y, double z);
    void updateSerialButtons(bool connected);
    void updateScanProgress(int current, int total);
    void loadFrequencyData();
    void populateFrequencyControls();
    void showHeatmap();
    bool refreshHeatmapPreview();
    void scheduleHeatmapPreviewRefresh();
    void updateHeatmapCursorReadout(double worldX, double worldY, bool insideImage);
    void applyHeatmapPreviewToCanvases();
    void updateColorbarDisplay();
    void updateOpacityLabel(int percent);
    void clearCurrentAnalyzer();
    void connectSpectrumAnalyzer();
    void disconnectSpectrumAnalyzer();
    void querySpectrumIdn();
    void applySpectrumConfig();
    void runSingleSpectrumSweep();
    void updateAnalyzerButtons(bool connected);
    void updateAnalyzerMethodHint(const QString &analyzerName);
    NFSScanner::Devices::Spectrum::SpectrumConfig readSpectrumConfig() const;
    double readFrequencyWithUnit(QLineEdit *edit, QComboBox *unitCombo) const;
    QString selectedDisplayMode() const;
    QString formatFrequency(double hz) const;
    QString resolveTraceCsvPath() const;
    AnalysisRenderParams buildAnalysisParams() const;
    void applyAlignmentToHeatmapView(const Core::AlignmentConfig &config);

    void refreshSerialPorts();
    void openSerialPort();
    void closeSerialPort();
    void jogAxis(const QString &axis, double direction);
    void resetPosition();
    void queryPosition();
    void readVersion();
    void readHelp();
    void executeAbsoluteMove();
    void setCurrentPositionAsScanPoint(bool startPoint);
    void syncStepInputsToTable();
    void startScan();
    void pauseScan();
    void stopScan();
    void advanceMockScan();
    void finishMockScan();
    void updateActionButtons();
    void setScanParamsLocked(bool locked);
    void previewScanPath();
    Core::ScanConfig readScanConfigFromUi() const;
    void applyPathPreviewToCanvas(const Core::ScanConfig &config, const QVector<Core::ScanPoint> &points);
    void saveAlignmentForTaskDir(const QString &taskDir, const Core::ScanConfig &config);
    QVector<ScanPoint> buildMockScanPoints() const;
    double scanTableValue(int column, double fallback) const;
    void setScanTableValue(int column, double value);

    double currentX_ = 0.0;
    double currentY_ = 0.0;
    double currentZ_ = 0.0;
    double jogStep_ = 1.0;
    QString appState_ = QStringLiteral("就绪");
    QString remainingText_ = QStringLiteral("--");
    QString estimatedFinishText_ = QStringLiteral("--");

    QTimer *clockTimer_ = nullptr;
    QTimer *mockScanTimer_ = nullptr;
    AnalysisController *analysisController_ = nullptr;
    NFSScanner::Core::DeviceManager *deviceManager_ = nullptr;
    NFSScanner::Project::ProjectManager *projectManager_ = nullptr;
    NFSScanner::License::LicenseManager *licenseManager_ = nullptr;
    NFSScanner::Core::ScanManager *scanManager_ = nullptr;
    NFSScanner::Devices::Motion::SerialMotionController *motionController_ = nullptr;
    int scanIndex_ = 0;
    QVector<ScanPoint> mockScanPoints_;

    QPlainTextEdit *logEdit_ = nullptr;
    QTableWidget *scanTable_ = nullptr;
    QLabel *deviceDiscoveryLabel_ = nullptr;
    QStatusBar *statusBar_ = nullptr;
    QLabel *statusTextLabel_ = nullptr;
    DeviceStatusBar *deviceStatusBar_ = nullptr;
    QProgressBar *scanProgressBar_ = nullptr;
    HeatmapView *heatmapView_ = nullptr;

    QComboBox *serialPortCombo_ = nullptr;
    QComboBox *baudRateCombo_ = nullptr;
    QPushButton *openSerialButton_ = nullptr;
    QPushButton *closeSerialButton_ = nullptr;
    QPushButton *refreshSerialButton_ = nullptr;
    QCheckBox *mockModeCheck_ = nullptr;

    QLineEdit *absoluteXEdit_ = nullptr;
    QLineEdit *absoluteYEdit_ = nullptr;
    QLineEdit *absoluteZEdit_ = nullptr;
    QLineEdit *feedEdit_ = nullptr;
    QLineEdit *projectNameEdit_ = nullptr;
    QLineEdit *testNameEdit_ = nullptr;
    QLineEdit *stepXEdit_ = nullptr;
    QLineEdit *stepYEdit_ = nullptr;
    QLineEdit *stepZEdit_ = nullptr;
    QLineEdit *resultDirEdit_ = nullptr;

    QPushButton *startScanButton_ = nullptr;
    QPushButton *pauseScanButton_ = nullptr;
    QPushButton *stopScanButton_ = nullptr;
    QCheckBox *snakeModeCheck_ = nullptr;
    QSpinBox *dwellTimeSpinBox_ = nullptr;
    QComboBox *traceCombo_ = nullptr;
    QComboBox *frequencyCombo_ = nullptr;
    QComboBox *displayModeCombo_ = nullptr;
    QComboBox *lutCombo_ = nullptr;
    QCheckBox *autoRangeCheck_ = nullptr;
    QDoubleSpinBox *vminSpin_ = nullptr;
    QDoubleSpinBox *vmaxSpin_ = nullptr;
    QSlider *opacitySlider_ = nullptr;
    QLabel *opacityLabel_ = nullptr;
    QLabel *colorbarLabel_ = nullptr;
    QLabel *colorbarMinLabel_ = nullptr;
    QLabel *colorbarMaxLabel_ = nullptr;
    QComboBox *analyzerTypeCombo_ = nullptr;
    QLabel *analyzerMethodLabel_ = nullptr;
    QLineEdit *analyzerHostEdit_ = nullptr;
    QLineEdit *analyzerPortEdit_ = nullptr;
    QPushButton *analyzerConnectButton_ = nullptr;
    QPushButton *analyzerDisconnectButton_ = nullptr;
    QPushButton *queryIdnButton_ = nullptr;
    QPushButton *applyAnalyzerConfigButton_ = nullptr;
    QPushButton *singleSweepButton_ = nullptr;
    QSpinBox *acquisitionTimeoutSpin_ = nullptr;
    QSpinBox *acquisitionRetrySpin_ = nullptr;
    QCheckBox *stopOnErrorCheck_ = nullptr;
    QLineEdit *startFreqEdit_ = nullptr;
    QLineEdit *stopFreqEdit_ = nullptr;
    QLineEdit *rbwEdit_ = nullptr;
    QLineEdit *sweepPointsEdit_ = nullptr;
    QComboBox *startFreqUnitCombo_ = nullptr;
    QComboBox *stopFreqUnitCombo_ = nullptr;
    QComboBox *rbwUnitCombo_ = nullptr;
    NFSScanner::Devices::Spectrum::ISpectrumAnalyzer *currentAnalyzer_ = nullptr;
    NFSScanner::Devices::Spectrum::SpectrumConfig currentSpectrumConfig_;
    NFSScanner::Devices::Spectrum::SpectrumTrace lastSpectrumTrace_;
    NFSScanner::Core::AlignmentManager alignmentManager_;
    AlignmentEditor *alignmentEditor_ = nullptr;

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
