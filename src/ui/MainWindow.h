#pragma once

#include <QMainWindow>
#include <QString>
#include <QVector>

class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStatusBar;
class QTableWidget;
class QTimer;
class QWidget;

namespace NFSScanner::UI {

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
    QGroupBox *createLogGroup();
    void setupStatusBar();

    void appendLog(const QString &text);
    void updateStatusBar();
    void setAppState(const QString &state);

    void refreshSerialPorts();
    void openSerialPort();
    void closeSerialPort();
    void jogAxis(const QString &axis, double direction);
    void resetPosition();
    void queryPosition();
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
    int scanIndex_ = 0;
    QVector<ScanPoint> mockScanPoints_;

    QPlainTextEdit *logEdit_ = nullptr;
    QTableWidget *scanTable_ = nullptr;
    QLabel *deviceDiscoveryLabel_ = nullptr;
    QStatusBar *statusBar_ = nullptr;
    QLabel *statusTextLabel_ = nullptr;

    QComboBox *serialPortCombo_ = nullptr;
    QComboBox *baudRateCombo_ = nullptr;
    QPushButton *openSerialButton_ = nullptr;
    QPushButton *closeSerialButton_ = nullptr;
    QPushButton *refreshSerialButton_ = nullptr;

    QLineEdit *absoluteXEdit_ = nullptr;
    QLineEdit *absoluteYEdit_ = nullptr;
    QLineEdit *absoluteZEdit_ = nullptr;
    QLineEdit *feedEdit_ = nullptr;
    QLineEdit *stepXEdit_ = nullptr;
    QLineEdit *stepYEdit_ = nullptr;
    QLineEdit *stepZEdit_ = nullptr;
    QLineEdit *resultDirEdit_ = nullptr;

    QPushButton *startScanButton_ = nullptr;
    QPushButton *pauseScanButton_ = nullptr;
    QPushButton *stopScanButton_ = nullptr;
};

} // namespace NFSScanner::UI
