#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVector>

class QThread;

namespace NFSScanner::Config {
class HardwareConfig;
}

namespace NFSScanner::Devices::Camera {
class ICamera;
}

namespace NFSScanner::Devices::Motion {
class IMotionController;
class MockMotionController;
class SerialMotionController;
}

namespace NFSScanner::Devices::Probe {
class IProbeController;
}

namespace NFSScanner::Devices::Spectrum {
class ISpectrumAnalyzer;
class SpectrumDeviceHost;
struct SpectrumConfig;
}

#include "config/HardwareConfig.h"
#include "devices/spectrum/SpectrumConfig.h"

namespace NFSScanner::Core {

enum class DeviceConnectionState {
    Disconnected,
    Connected,
    Mock,
    Fault
};

QString deviceConnectionStateText(DeviceConnectionState state);

struct DeviceHealthItem
{
    QString name;
    bool ok = false;
    QString message;
};

struct DeviceHealth
{
    QVector<DeviceHealthItem> items;

    bool overallOk() const
    {
        for (const DeviceHealthItem &item : items) {
            if (!item.ok) {
                return false;
            }
        }
        return true;
    }
};

class DeviceManager final : public QObject
{
    Q_OBJECT

public:
    explicit DeviceManager(QObject *parent = nullptr);
    ~DeviceManager() override;

    Config::HardwareConfig hardwareConfig() const;
    bool loadHardwareConfig(const QString &path = QString());
    bool saveHardwareConfig(const QString &path = QString());
    void setHardwareConfig(const Config::HardwareConfig &config);
    bool loadHardwareProfile(const QString &profileName);
    bool saveHardwareProfile(const QString &profileName);
    QString hardwareProfileName() const { return hardwareProfileName_; }

    NFSScanner::Devices::Motion::SerialMotionController *motionController();
    NFSScanner::Devices::Motion::IMotionController *motionInterface();

    NFSScanner::Devices::Spectrum::ISpectrumAnalyzer *spectrumAnalyzer() const;
    QThread *spectrumDeviceThread() const;
    NFSScanner::Devices::Spectrum::SpectrumDeviceHost *spectrumDeviceHost() const;
    NFSScanner::Devices::Camera::ICamera *camera() const;
    NFSScanner::Devices::Probe::IProbeController *probeController() const;

    void setMotionMockMode(bool enabled);
    bool motionMockMode() const;

    bool connectMotion();
    void disconnectMotion();
    bool isMotionConnected() const;

    bool connectSpectrum();
    void disconnectSpectrum();
    bool isSpectrumConnected() const;

    bool connectCamera(bool requireEnabled = true);
    void disconnectCamera();
    bool isCameraConnected() const;

    bool connectProbe();
    void disconnectProbe();
    bool isProbeConnected() const;

    bool createSpectrumAnalyzer(const QString &analyzerName);
    bool connectSpectrumAnalyzer(const QVariantMap &options);
    void disconnectSpectrumAnalyzer();
    void releaseSpectrumAnalyzer();
    bool configureSpectrum(const NFSScanner::Devices::Spectrum::SpectrumConfig &config);
    QString querySpectrumIdn();

    void refreshDevices();
    bool connectAll();
    void disconnectAll();

    DeviceConnectionState motionState() const;
    DeviceConnectionState spectrumState() const;
    DeviceConnectionState cameraState() const;
    DeviceConnectionState probeState() const;

    DeviceHealth runHealthCheck();
    QString lastError() const;

signals:
    void logMessage(const QString &message);
    void deviceStateChanged();
    void motionMockModeChanged(bool mock);
    void hardwareConfigChanged();

private:
    void setLastError(const QString &message);
    void updateMotionState();
    void updateSpectrumState();
    void releaseProbeController();
    void releaseCamera();
    bool ensureSpectrumAnalyzerFromConfig();
    NFSScanner::Devices::Spectrum::SpectrumConfig spectrumConfigFromHardware() const;

    Config::HardwareConfig hardwareConfig_;
    QString hardwareConfigPath_;
    QString hardwareProfileName_;

    NFSScanner::Devices::Motion::SerialMotionController *serialMotion_ = nullptr;
    NFSScanner::Devices::Motion::MockMotionController *mockMotion_ = nullptr;
    QThread *spectrumDeviceThread_ = nullptr;
    NFSScanner::Devices::Spectrum::SpectrumDeviceHost *spectrumDeviceHost_ = nullptr;
    NFSScanner::Devices::Spectrum::ISpectrumAnalyzer *spectrumAnalyzer_ = nullptr;
    NFSScanner::Devices::Camera::ICamera *camera_ = nullptr;
    NFSScanner::Devices::Probe::IProbeController *probe_ = nullptr;

    bool motionMockMode_ = true;
    DeviceConnectionState motionState_ = DeviceConnectionState::Mock;
    DeviceConnectionState spectrumState_ = DeviceConnectionState::Disconnected;
    DeviceConnectionState cameraState_ = DeviceConnectionState::Disconnected;
    DeviceConnectionState probeState_ = DeviceConnectionState::Disconnected;
    QString lastError_;
};

} // namespace NFSScanner::Core
