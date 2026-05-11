#pragma once

#include <QMainWindow>
#include <QString>

#include <memory>

#include "core/ScanPoint.h"

class QAction;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QWidget;

namespace NFSScanner::App {
class AppContext;
}

namespace NFSScanner::Core {
struct ScanConfig;
}

namespace NFSScanner::UI {

class HeatmapView;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void handleStartScan();
    void handleStopScan();
    void handleScanProgress(int currentPoint, int totalPoints, const NFSScanner::Core::ScanPoint &point);
    void handleScanFinished(bool completed);
    void appendLog(const QString &message);

private:
    void setupMenus();
    void setupCentralUi();
    void setupConnections();
    QWidget *createLeftPanel();
    QWidget *createRightPanel();
    NFSScanner::Core::ScanConfig buildScanConfig() const;
    void setScanControlsEnabled(bool running);
    void connectMockDevices();
    void disconnectMockDevices();

    std::unique_ptr<NFSScanner::App::AppContext> appContext_;

    QAction *startScanAction_ = nullptr;
    QAction *stopScanAction_ = nullptr;
    QAction *connectDevicesAction_ = nullptr;
    QAction *disconnectDevicesAction_ = nullptr;

    HeatmapView *heatmapView_ = nullptr;
    QPlainTextEdit *logEdit_ = nullptr;
    QProgressBar *progressBar_ = nullptr;

    QPushButton *connectDevicesButton_ = nullptr;
    QPushButton *disconnectDevicesButton_ = nullptr;
    QPushButton *startScanButton_ = nullptr;
    QPushButton *stopScanButton_ = nullptr;
    QPushButton *moveButton_ = nullptr;

    QLabel *motionStatusLabel_ = nullptr;
    QLabel *spectrumStatusLabel_ = nullptr;
    QLabel *motionPositionLabel_ = nullptr;

    QSpinBox *columnsSpin_ = nullptr;
    QSpinBox *rowsSpin_ = nullptr;
    QDoubleSpinBox *stepSpin_ = nullptr;
    QDoubleSpinBox *startFrequencySpin_ = nullptr;
    QDoubleSpinBox *stopFrequencySpin_ = nullptr;
    QDoubleSpinBox *motionXSpin_ = nullptr;
    QDoubleSpinBox *motionYSpin_ = nullptr;
    QDoubleSpinBox *motionZSpin_ = nullptr;
    QDoubleSpinBox *centerFrequencySpin_ = nullptr;
    QComboBox *traceCombo_ = nullptr;

    bool scanRunning_ = false;
};

} // namespace NFSScanner::UI
