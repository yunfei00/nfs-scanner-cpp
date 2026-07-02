#pragma once

#include "devices/spectrum/SpectrumConfig.h"
#include "devices/spectrum/SpectrumTrace.h"
#include "devices/spectrum/DeviceBringupResult.h"

#include <QWidget>

#include <functional>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

namespace NFSScanner::Core {
class DeviceManager;
class ScanManager;
}

namespace NFSScanner::Devices::Motion {
class SerialMotionController;
}

namespace NFSScanner::Devices::Spectrum {
class ISpectrumAnalyzer;
}

namespace NFSScanner::UI {

class DevicePage final : public QWidget
{
    Q_OBJECT

public:
    explicit DevicePage(QWidget *parent = nullptr);

    QWidget *contentHost() const;
    QWidget *paramPanel() const;

    void bind(NFSScanner::Core::DeviceManager *deviceManager,
              NFSScanner::Core::ScanManager *scanManager,
              NFSScanner::Devices::Motion::SerialMotionController *motionController,
              QPlainTextEdit *logEdit,
              QWidget *messageBoxParent);

    using StateSetter = std::function<void(const QString &)>;
    using PositionChangedHandler = std::function<void(double, double, double)>;
    using DwellMsProvider = std::function<int()>;

    void setStateSetter(StateSetter setter);
    void setPositionChangedHandler(PositionChangedHandler handler);
    void setDwellMsProvider(DwellMsProvider provider);

    bool isMockMode() const;
    double feedValue() const;
    NFSScanner::Devices::Spectrum::ISpectrumAnalyzer *currentAnalyzer() const;
    NFSScanner::Devices::Spectrum::SpectrumConfig currentSpectrumConfig() const;
    NFSScanner::Devices::Spectrum::SpectrumConfig readSpectrumConfig() const;
    int acquisitionTimeoutMs() const;
    int acquisitionRetryCount() const;
    bool stopOnAcquisitionError() const;
    bool isNonMockAnalyzerSelected() const;
    QLabel *deviceDiscoveryLabel() const;

    void refreshSerialPorts();
    void clearCurrentAnalyzer();

signals:
    void logMessage(const QString &text);
    void analyzerConnectedChanged(bool connected);

private:
    void buildContentHost();
    QWidget *buildParamPanel();
    QGroupBox *createSerialGroup();
    QGroupBox *createMotionControlGroup();
    QGroupBox *createMotionCommandGroup();
    QGroupBox *createInstrumentGroup();
    void appendLog(const QString &text);
    void notifyPositionChanged();
    void openSerialPort();
    void closeSerialPort();
    void jogAxis(const QString &axis, double direction);
    void resetPosition();
    void queryPosition();
    void readVersion();
    void readHelp();
    void executeAbsoluteMove();
    void updateSerialButtons(bool connected);
    void connectSpectrumAnalyzer();
    void disconnectSpectrumAnalyzer();
    void querySpectrumIdn();
    void applySpectrumConfig();
    void runSingleSpectrumSweep();
    void updateAnalyzerButtons(bool connected);
    void updateAnalyzerMethodHint(const QString &analyzerName);
    QGroupBox *createHardwareConfigGroup();
    QGroupBox *createDeviceTestGroup();
    void loadHardwareConfigToUi();
    void saveHardwareConfigFromUi();
    double readFrequencyWithUnit(QLineEdit *edit, QComboBox *unitCombo) const;
    bool ensureRealMotionReady();
    bool validateMotionTarget(double x, double y, double z);

    QWidget *contentHost_ = nullptr;
    QWidget *paramPanel_ = nullptr;

    NFSScanner::Core::DeviceManager *deviceManager_ = nullptr;
    NFSScanner::Core::ScanManager *scanManager_ = nullptr;
    NFSScanner::Devices::Motion::SerialMotionController *motionController_ = nullptr;
    QPlainTextEdit *logEdit_ = nullptr;
    QWidget *messageBoxParent_ = nullptr;

    StateSetter stateSetter_;
    PositionChangedHandler positionChangedHandler_;
    DwellMsProvider dwellMsProvider_;

    double currentX_ = 0.0;
    double currentY_ = 0.0;
    double currentZ_ = 0.0;
    double jogStep_ = 1.0;

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
    QLabel *deviceDiscoveryLabel_ = nullptr;

    QCheckBox *hwMotionEnabledCheck_ = nullptr;
    QLineEdit *hwMotionPortEdit_ = nullptr;
    QCheckBox *hwSpectrumEnabledCheck_ = nullptr;
    QComboBox *hwSpectrumTypeCombo_ = nullptr;
    QLineEdit *hwSpectrumHostEdit_ = nullptr;
    QLineEdit *hwSpectrumPortEdit_ = nullptr;
    QCheckBox *hwCameraEnabledCheck_ = nullptr;
    QComboBox *hwCameraTypeCombo_ = nullptr;
    QCheckBox *hwProbeEnabledCheck_ = nullptr;
    QComboBox *hwProbeTypeCombo_ = nullptr;
    QComboBox *hwProbeOrientationCombo_ = nullptr;
    QComboBox *hwProfileCombo_ = nullptr;

    NFSScanner::Devices::Spectrum::ISpectrumAnalyzer *currentAnalyzer_ = nullptr;
    NFSScanner::Devices::Spectrum::SpectrumConfig currentSpectrumConfig_;
    NFSScanner::Devices::Spectrum::SpectrumTrace lastSpectrumTrace_;
    NFSScanner::Devices::Spectrum::DeviceBringupResult lastBringupResult_;
};

} // namespace NFSScanner::UI
