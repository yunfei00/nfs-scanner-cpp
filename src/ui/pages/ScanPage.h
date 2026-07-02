#pragma once

#include "core/AlignmentConfig.h"
#include "core/ScanConfig.h"
#include "core/ScanPoint.h"

#include <QString>
#include <QVector>
#include <QWidget>

#include <functional>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTimer;

namespace NFSScanner::Core {
class AlignmentManager;
class ScanManager;
}

namespace NFSScanner::Project {
class ProjectManager;
}

namespace NFSScanner::UI {

class AlignmentEditor;
class HeatmapView;

struct MockScanPoint
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

class ScanPage final : public QWidget
{
    Q_OBJECT

public:
    explicit ScanPage(QWidget *parent = nullptr);

    HeatmapView *canvas() const;
    QWidget *paramPanel() const;
    AlignmentEditor *alignmentEditor() const;

    void bind(NFSScanner::Core::ScanManager *scanManager,
              HeatmapView *heatmapView,
              AlignmentEditor *alignmentEditor,
              NFSScanner::Core::AlignmentManager *alignmentManager,
              NFSScanner::Project::ProjectManager *projectManager,
              QPlainTextEdit *logEdit,
              QWidget *messageBoxParent);

    using FeedProvider = std::function<double()>;
    using BoolProvider = std::function<bool()>;
    using StringProvider = std::function<QString()>;
    using ScanLaunchHandler = std::function<bool(NFSScanner::Core::ScanManager *, const NFSScanner::Core::ScanConfig &)>;
    using PageChecker = std::function<bool()>;
    using PreScanHandler = std::function<bool(const NFSScanner::Core::ScanConfig &, int pointCount, const QString &plannerError)>;

    void setFeedProvider(FeedProvider provider);
    void setMockModeChecker(BoolProvider checker);
    void setMotionReadyChecker(BoolProvider checker);
    void setScanLaunchHandler(ScanLaunchHandler handler);
    void setPreScanHandler(PreScanHandler handler);
    void setOnScanPageChecker(PageChecker checker);
    void setResultDirProvider(StringProvider provider);
    void setCurrentPosition(double x, double y, double z);

    NFSScanner::Core::ScanConfig readScanConfigFromUi() const;
    void startScan();
    void pauseScan();
    void stopScan();
    void previewScanPath();
    void setScanParamsLocked(bool locked);
    void saveAlignmentForTaskDir(const QString &taskDir, const NFSScanner::Core::ScanConfig &config);
    void applyPathPreviewToCanvas(const NFSScanner::Core::ScanConfig &config, const QVector<NFSScanner::Core::ScanPoint> &points);
    void syncStepInputsToTable();
    void setCurrentPositionAsScanPoint(bool startPoint);
    void updateScanProgress(int current, int total);
    void updateActionButtons();
    void setHardwareModeText(const QString &text);
    QVector<MockScanPoint> buildMockScanPoints() const;

public slots:
    void advanceMockScan();
    void finishMockScan();

signals:
    void logMessage(const QString &text);
    void requestSwitchToScanPage();
    void mockScanPositionChanged(double x, double y, double z);
    void mockScanProgressChanged(int remainingCount, int estimatedSeconds);
    void mockScanStateChanged(const QString &state);
    void mockScanFinished();

private:
    QWidget *buildParamPanel();
    QGroupBox *createScanAreaGroup();
    QGroupBox *createTestInfoGroup();
    QGroupBox *createStepConfigGroup();
    QGroupBox *createActionGroup();
    void appendLog(const QString &text);
    double scanTableValue(int column, double fallback) const;
    void setScanTableValue(int column, double value);
    void applyAlignmentToHeatmapView(const NFSScanner::Core::AlignmentConfig &config);

    HeatmapView *canvas_ = nullptr;
    QWidget *paramPanel_ = nullptr;

    NFSScanner::Core::ScanManager *scanManager_ = nullptr;
    HeatmapView *heatmapView_ = nullptr;
    AlignmentEditor *alignmentEditor_ = nullptr;
    NFSScanner::Core::AlignmentManager *alignmentManager_ = nullptr;
    NFSScanner::Project::ProjectManager *projectManager_ = nullptr;
    QPlainTextEdit *logEdit_ = nullptr;
    QWidget *messageBoxParent_ = nullptr;

    FeedProvider feedProvider_;
    BoolProvider mockModeChecker_;
    BoolProvider motionReadyChecker_;
    ScanLaunchHandler scanLaunchHandler_;
    PreScanHandler preScanHandler_;
    PageChecker onScanPageChecker_;
    StringProvider resultDirProvider_;

    double currentX_ = 0.0;
    double currentY_ = 0.0;
    double currentZ_ = 0.0;

    QTimer *mockScanTimer_ = nullptr;
    int scanIndex_ = 0;
    QVector<MockScanPoint> mockScanPoints_;

    QTableWidget *scanTable_ = nullptr;
    QLineEdit *projectNameEdit_ = nullptr;
    QLineEdit *testNameEdit_ = nullptr;
    QComboBox *probeOrientationCombo_ = nullptr;
    QLineEdit *stepXEdit_ = nullptr;
    QLineEdit *stepYEdit_ = nullptr;
    QLineEdit *stepZEdit_ = nullptr;
    QPushButton *startScanButton_ = nullptr;
    QPushButton *pauseScanButton_ = nullptr;
    QPushButton *stopScanButton_ = nullptr;
    QCheckBox *snakeModeCheck_ = nullptr;
    QSpinBox *dwellTimeSpinBox_ = nullptr;
    QProgressBar *scanProgressBar_ = nullptr;
    QLabel *hardwareModeLabel_ = nullptr;
};

} // namespace NFSScanner::UI
