#include "core/DeviceManager.h"

#include "devices/camera/MockCamera.h"
#include "devices/motion/SerialMotionController.h"
#include "devices/spectrum/ISpectrumAnalyzer.h"
#include "devices/spectrum/SpectrumDeviceHost.h"

#include <QMetaObject>
#include <QThread>

namespace NFSScanner::Core {

QString deviceConnectionStateText(DeviceConnectionState state)
{
    switch (state) {
    case DeviceConnectionState::Disconnected:
        return QStringLiteral("未连接");
    case DeviceConnectionState::Connected:
        return QStringLiteral("已连接");
    case DeviceConnectionState::Mock:
        return QStringLiteral("模拟");
    case DeviceConnectionState::Fault:
        return QStringLiteral("故障");
    }
    return QStringLiteral("未知");
}

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent)
    , motionController_(new Devices::Motion::SerialMotionController(this))
    , spectrumDeviceThread_(new QThread(this))
    , spectrumDeviceHost_(new Devices::Spectrum::SpectrumDeviceHost)
{
    connect(motionController_, &Devices::Motion::SerialMotionController::logMessage,
            this, &DeviceManager::logMessage);
    connect(motionController_, &Devices::Motion::SerialMotionController::connectedChanged,
            this, [this](bool connected) {
                motionState_ = motionMockMode_
                    ? DeviceConnectionState::Mock
                    : (connected ? DeviceConnectionState::Connected : DeviceConnectionState::Disconnected);
                emit deviceStateChanged();
            });

    spectrumDeviceHost_->moveToThread(spectrumDeviceThread_);
    connect(spectrumDeviceHost_, &Devices::Spectrum::SpectrumDeviceHost::logMessage,
            this, &DeviceManager::logMessage);
    connect(spectrumDeviceHost_, &Devices::Spectrum::SpectrumDeviceHost::connectedChanged,
            this, [this](bool connected) {
                spectrumState_ = connected ? DeviceConnectionState::Connected : DeviceConnectionState::Disconnected;
                emit deviceStateChanged();
            });
    connect(spectrumDeviceThread_, &QThread::finished, spectrumDeviceHost_, &QObject::deleteLater);
    spectrumDeviceThread_->start();

    updateMotionState();
}

DeviceManager::~DeviceManager()
{
    releaseSpectrumAnalyzer();
    disconnectCamera();

    if (spectrumDeviceThread_) {
        spectrumDeviceThread_->quit();
        spectrumDeviceThread_->wait(3000);
    }
    spectrumDeviceHost_ = nullptr;
}

NFSScanner::Devices::Motion::SerialMotionController *DeviceManager::motionController()
{
    return motionController_;
}

NFSScanner::Devices::Motion::IMotionController *DeviceManager::motionInterface()
{
    return motionController_;
}

NFSScanner::Devices::Spectrum::ISpectrumAnalyzer *DeviceManager::spectrumAnalyzer() const
{
    return spectrumAnalyzer_;
}

QThread *DeviceManager::spectrumDeviceThread() const
{
    return spectrumDeviceThread_;
}

NFSScanner::Devices::Spectrum::SpectrumDeviceHost *DeviceManager::spectrumDeviceHost() const
{
    return spectrumDeviceHost_;
}

NFSScanner::Devices::Camera::ICamera *DeviceManager::camera() const
{
    return camera_;
}

void DeviceManager::setMotionMockMode(bool enabled)
{
    if (motionMockMode_ == enabled) {
        return;
    }
    motionMockMode_ = enabled;
    if (enabled && motionController_ && motionController_->isOpen()) {
        motionController_->closePort();
    }
    updateMotionState();
    emit motionMockModeChanged(enabled);
    emit deviceStateChanged();
}

bool DeviceManager::motionMockMode() const
{
    return motionMockMode_;
}

bool DeviceManager::createSpectrumAnalyzer(const QString &analyzerName)
{
    if (!spectrumDeviceHost_) {
        setLastError(QStringLiteral("频谱设备线程未就绪。"));
        return false;
    }

    bool ok = false;
    QMetaObject::invokeMethod(spectrumDeviceHost_, "releaseAnalyzer", Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(spectrumDeviceHost_, "createAnalyzer", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, ok),
                              Q_ARG(QString, analyzerName));
    if (!ok) {
        QString hostError;
        QMetaObject::invokeMethod(spectrumDeviceHost_, "lastError", Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(QString, hostError));
        setLastError(hostError.isEmpty() ? QStringLiteral("无法创建频谱仪：%1").arg(analyzerName) : hostError);
        spectrumAnalyzer_ = nullptr;
        spectrumState_ = DeviceConnectionState::Fault;
        emit deviceStateChanged();
        return false;
    }

    spectrumAnalyzer_ = spectrumDeviceHost_->analyzer();
    spectrumState_ = DeviceConnectionState::Disconnected;
    emit deviceStateChanged();
    return true;
}

bool DeviceManager::connectSpectrumAnalyzer(const QVariantMap &options)
{
    if (!spectrumDeviceHost_) {
        setLastError(QStringLiteral("频谱设备线程未就绪。"));
        return false;
    }

    bool ok = false;
    QMetaObject::invokeMethod(spectrumDeviceHost_, "connectDevice", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, ok),
                              Q_ARG(QVariantMap, options));
    if (!ok) {
        QString hostError;
        QMetaObject::invokeMethod(spectrumDeviceHost_, "lastError", Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(QString, hostError));
        setLastError(hostError.isEmpty() ? QStringLiteral("频谱仪连接失败。") : hostError);
        return false;
    }

    spectrumAnalyzer_ = spectrumDeviceHost_->analyzer();
    setLastError({});
    return true;
}

void DeviceManager::disconnectSpectrumAnalyzer()
{
    if (!spectrumDeviceHost_) {
        return;
    }
    QMetaObject::invokeMethod(spectrumDeviceHost_, "disconnectDevice", Qt::BlockingQueuedConnection);
}

void DeviceManager::releaseSpectrumAnalyzer()
{
    if (!spectrumDeviceHost_) {
        spectrumAnalyzer_ = nullptr;
        return;
    }

    QMetaObject::invokeMethod(spectrumDeviceHost_, "releaseAnalyzer", Qt::BlockingQueuedConnection);
    spectrumAnalyzer_ = nullptr;
    spectrumState_ = DeviceConnectionState::Disconnected;
    emit deviceStateChanged();
}

bool DeviceManager::configureSpectrum(const NFSScanner::Devices::Spectrum::SpectrumConfig &config)
{
    if (!spectrumDeviceHost_) {
        setLastError(QStringLiteral("频谱设备线程未就绪。"));
        return false;
    }

    bool ok = false;
    QMetaObject::invokeMethod(spectrumDeviceHost_, "configureDevice", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, ok),
                              Q_ARG(NFSScanner::Devices::Spectrum::SpectrumConfig, config));
    if (!ok) {
        QString hostError;
        QMetaObject::invokeMethod(spectrumDeviceHost_, "lastError", Qt::BlockingQueuedConnection,
                                  Q_RETURN_ARG(QString, hostError));
        setLastError(hostError);
    }
    return ok;
}

QString DeviceManager::querySpectrumIdn()
{
    if (!spectrumDeviceHost_) {
        return {};
    }

    QString idn;
    QMetaObject::invokeMethod(spectrumDeviceHost_, "queryIdn", Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QString, idn));
    return idn;
}

bool DeviceManager::connectCamera(const QString &cameraType)
{
    disconnectCamera();
    if (cameraType.contains(QStringLiteral("Mock"), Qt::CaseInsensitive)) {
        camera_ = new Devices::Camera::MockCamera(this);
    } else {
        // TODO(camera): USB / industrial camera drivers without mandatory OpenCV.
        setLastError(QStringLiteral("仅 Mock Camera 可用，%1 尚未实现。").arg(cameraType));
        return false;
    }

    connect(camera_, &Devices::Camera::ICamera::logMessage, this, &DeviceManager::logMessage);
    connect(camera_, &Devices::Camera::ICamera::connectedChanged, this, [this](bool connected) {
        cameraState_ = connected ? DeviceConnectionState::Mock : DeviceConnectionState::Disconnected;
        emit deviceStateChanged();
    });

    if (!camera_->connectDevice(QVariantMap{})) {
        camera_->deleteLater();
        camera_ = nullptr;
        cameraState_ = DeviceConnectionState::Fault;
        emit deviceStateChanged();
        return false;
    }
    cameraState_ = DeviceConnectionState::Mock;
    emit deviceStateChanged();
    return true;
}

void DeviceManager::disconnectCamera()
{
    if (!camera_) {
        cameraState_ = DeviceConnectionState::Disconnected;
        return;
    }
    camera_->disconnectDevice();
    camera_->deleteLater();
    camera_ = nullptr;
    cameraState_ = DeviceConnectionState::Disconnected;
    emit deviceStateChanged();
}

void DeviceManager::refreshDevices()
{
    emit logMessage(QStringLiteral("设备刷新：串口列表请在设备页点击「刷新串口」。"));
    emit deviceStateChanged();
}

bool DeviceManager::connectAll()
{
    bool ok = true;
    if (spectrumAnalyzer_ && !spectrumAnalyzer_->isConnected()) {
        ok = false;
        emit logMessage(QStringLiteral("频谱仪需先配置 Host/Port 并在设备页连接。"));
    }
    if (!camera_ || !camera_->isConnected()) {
        if (!connectCamera()) {
            ok = false;
        }
    }
    emit logMessage(QStringLiteral("连接全部完成（运动平台需单独打开串口）。"));
    return ok;
}

void DeviceManager::disconnectAll()
{
    if (motionController_ && motionController_->isOpen()) {
        motionController_->closePort();
    }
    disconnectSpectrumAnalyzer();
    disconnectCamera();
    emit logMessage(QStringLiteral("已断开全部设备。"));
}

DeviceConnectionState DeviceManager::motionState() const
{
    return motionState_;
}

DeviceConnectionState DeviceManager::spectrumState() const
{
    return spectrumState_;
}

DeviceConnectionState DeviceManager::cameraState() const
{
    return cameraState_;
}

QString DeviceManager::lastError() const
{
    return lastError_;
}

void DeviceManager::setLastError(const QString &message)
{
    lastError_ = message;
}

void DeviceManager::updateMotionState()
{
    motionState_ = motionMockMode_
        ? DeviceConnectionState::Mock
        : (motionController_ && motionController_->isOpen()
               ? DeviceConnectionState::Connected
               : DeviceConnectionState::Disconnected);
}

} // namespace NFSScanner::Core
