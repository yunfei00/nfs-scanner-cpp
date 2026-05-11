#pragma once

#include "analysis/FrequencyData.h"

#include <QMainWindow>
#include <QString>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSlider;
class QStatusBar;
class QSpinBox;
class QTableWidget;
class QTimer;
class QWidget;

namespace NFSScanner::Core {
class ScanManager;
}

namespace NFSScanner::Devices::Motion {
class SerialMotionController;
}

namespace NFSScanner::UI {

class HeatmapView;

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

private:
    void setupUi();
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
    void updateColorbarDisplay();
    void updateOpacityLabel(int percent);
    QString selectedDisplayMode() const;
    QString formatFrequency(double hz) const;
    QString resolveTraceCsvPath() const;

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
    NFSScanner::Core::ScanManager *scanManager_ = nullptr;
    NFSScanner::Devices::Motion::SerialMotionController *motionController_ = nullptr;
    int scanIndex_ = 0;
    QVector<ScanPoint> mockScanPoints_;

    QPlainTextEdit *logEdit_ = nullptr;
    QTableWidget *scanTable_ = nullptr;
    QLabel *deviceDiscoveryLabel_ = nullptr;
    QStatusBar *statusBar_ = nullptr;
    QLabel *statusTextLabel_ = nullptr;
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
    NFSScanner::Analysis::FrequencyData frequencyData_;
    QImage currentHeatmapImage_;
    QImage currentColorbarImage_;
    double currentVmin_ = 0.0;
    double currentVmax_ = 1.0;
};

} // namespace NFSScanner::UI
