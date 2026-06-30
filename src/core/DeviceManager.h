#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class QThread;

namespace NFSScanner::Devices::Camera {
class ICamera;
}

namespace NFSScanner::Devices::Motion {
class IMotionController;
class SerialMotionController;
}

namespace NFSScanner::Devices::Spectrum {
class ISpectrumAnalyzer;
class SpectrumDeviceHost;
}

#include "devices/spectrum/SpectrumConfig.h"

namespace NFSScanner::Core {

enum class DeviceConnectionState {
    Disconnected,
    Connected,
    Mock,
    Fault
};

QString deviceConnectionStateText(DeviceConnectionState state);

class DeviceManager final : public QObject
{
    Q_OBJECT

public:
    explicit DeviceManager(QObject *parent = nullptr);
    ~DeviceManager() override;

    NFSScanner::Devices::Motion::SerialMotionController *motionController();
    NFSScanner::Devices::Motion::IMotionController *motionInterface();

    NFSScanner::Devices::Spectrum::ISpectrumAnalyzer *spectrumAnalyzer() const;
    QThread *spectrumDeviceThread() const;
    NFSScanner::Devices::Spectrum::SpectrumDeviceHost *spectrumDeviceHost() const;
    NFSScanner::Devices::Camera::ICamera *camera() const;

    void setMotionMockMode(bool enabled);
    bool motionMockMode() const;

    bool createSpectrumAnalyzer(const QString &analyzerName);
    bool connectSpectrumAnalyzer(const QVariantMap &options);
    void disconnectSpectrumAnalyzer();
    void releaseSpectrumAnalyzer();
    bool configureSpectrum(const NFSScanner::Devices::Spectrum::SpectrumConfig &config);
    QString querySpectrumIdn();

    bool connectCamera(const QString &cameraType = QStringLiteral("Mock Camera"));
    void disconnectCamera();

    void refreshDevices();
    bool connectAll();
    void disconnectAll();

    DeviceConnectionState motionState() const;
    DeviceConnectionState spectrumState() const;
    DeviceConnectionState cameraState() const;

    QString lastError() const;

signals:
    void logMessage(const QString &message);
    void deviceStateChanged();
    void motionMockModeChanged(bool mock);

private:
    void setLastError(const QString &message);
    void updateMotionState();

    NFSScanner::Devices::Motion::SerialMotionController *motionController_ = nullptr;
    QThread *spectrumDeviceThread_ = nullptr;
    NFSScanner::Devices::Spectrum::SpectrumDeviceHost *spectrumDeviceHost_ = nullptr;
    NFSScanner::Devices::Spectrum::ISpectrumAnalyzer *spectrumAnalyzer_ = nullptr;
    NFSScanner::Devices::Camera::ICamera *camera_ = nullptr;
    bool motionMockMode_ = true;
    DeviceConnectionState motionState_ = DeviceConnectionState::Mock;
    DeviceConnectionState spectrumState_ = DeviceConnectionState::Disconnected;
    DeviceConnectionState cameraState_ = DeviceConnectionState::Disconnected;
    QString lastError_;
};

} // namespace NFSScanner::Core
