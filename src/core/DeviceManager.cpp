#include "core/DeviceManager.h"

#include "config/HardwareConfigManager.h"
#include "devices/camera/CameraFactory.h"
#include "devices/motion/MockMotionController.h"
#include "devices/motion/SerialMotionController.h"
#include "devices/probe/ProbeControllerFactory.h"
#include "devices/spectrum/ISpectrumAnalyzer.h"
#include "devices/spectrum/SpectrumDeviceHost.h"
#include "infra/Logger.h"

#include <QFile>
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
    , serialMotion_(new Devices::Motion::SerialMotionController(this))
    , mockMotion_(new Devices::Motion::MockMotionController(this))
    , spectrumDeviceThread_(new QThread(this))
    , spectrumDeviceHost_(new Devices::Spectrum::SpectrumDeviceHost)
{
    hardwareConfig_ = Config::HardwareConfig::defaults();
    hardwareConfigPath_ = Config::defaultHardwareConfigPath();
    loadHardwareConfig(hardwareConfigPath_);

    connect(serialMotion_, &Devices::Motion::SerialMotionController::logMessage,
            this, &DeviceManager::logMessage);
    connect(serialMotion_, &Devices::Motion::SerialMotionController::connectedChanged,
            this, [this](bool connected) {
                if (!motionMockMode_) {
                    motionState_ = connected ? DeviceConnectionState::Connected
                                             : DeviceConnectionState::Disconnected;
                    emit deviceStateChanged();
                }
            });
    connect(mockMotion_, &Devices::Motion::MockMotionController::connectionChanged,
            this, [this](bool connected) {
                if (motionMockMode_) {
                    motionState_ = connected ? DeviceConnectionState::Mock
                                             : DeviceConnectionState::Disconnected;
                    emit deviceStateChanged();
                }
            });

    spectrumDeviceHost_->moveToThread(spectrumDeviceThread_);
    connect(spectrumDeviceHost_, &Devices::Spectrum::SpectrumDeviceHost::logMessage,
            this, &DeviceManager::logMessage);
    connect(spectrumDeviceHost_, &Devices::Spectrum::SpectrumDeviceHost::connectedChanged,
            this, [this](bool connected) {
                spectrumState_ = connected ? DeviceConnectionState::Connected
                                           : DeviceConnectionState::Disconnected;
                emit deviceStateChanged();
            });
    connect(spectrumDeviceThread_, &QThread::finished, spectrumDeviceHost_, &QObject::deleteLater);
    spectrumDeviceThread_->start();

    updateMotionState();
}

DeviceManager::~DeviceManager()
{
    releaseSpectrumAnalyzer();
    disconnectProbe();
    disconnectCamera();

    if (spectrumDeviceThread_) {
        spectrumDeviceThread_->quit();
        spectrumDeviceThread_->wait(3000);
    }
    spectrumDeviceHost_ = nullptr;
}

Config::HardwareConfig DeviceManager::hardwareConfig() const
{
    return hardwareConfig_;
}

bool DeviceManager::loadHardwareConfig(const QString &path)
{
    Config::HardwareConfigManager manager;
    const QString targetPath = path.isEmpty() ? hardwareConfigPath_ : path;
    if (!manager.load(targetPath)) {
        setLastError(manager.lastError());
        Infra::Logger::appendGlobalHistory(Infra::Logger::format(Infra::Logger::Level::Error, manager.lastError()));
        return false;
    }

    hardwareConfig_ = manager.config();
    hardwareConfigPath_ = manager.configPath();
    emit hardwareConfigChanged();
    emit logMessage(QStringLiteral("硬件配置已加载: %1").arg(hardwareConfigPath_));
    return true;
}

bool DeviceManager::saveHardwareConfig(const QString &path)
{
    Config::HardwareConfigManager manager;
    manager.config() = hardwareConfig_;
    manager.setConfigPath(path.isEmpty() ? hardwareConfigPath_ : path);
    if (!manager.save()) {
        setLastError(manager.lastError());
        return false;
    }
    hardwareConfigPath_ = manager.configPath();
    emit hardwareConfigChanged();
    emit logMessage(QStringLiteral("硬件配置已保存: %1").arg(hardwareConfigPath_));
    return true;
}

void DeviceManager::setHardwareConfig(const Config::HardwareConfig &config)
{
    hardwareConfig_ = config;
    emit hardwareConfigChanged();
}

bool DeviceManager::loadHardwareProfile(const QString &profileName)
{
    Config::HardwareConfigManager manager;
    manager.config() = hardwareConfig_;
    if (!manager.loadProfile(profileName)) {
        setLastError(manager.lastError());
        return false;
    }
    hardwareConfig_ = manager.config();
    hardwareProfileName_ = manager.currentProfileName();
    emit hardwareConfigChanged();
    emit logMessage(QStringLiteral("已加载硬件 Profile: %1").arg(hardwareProfileName_));
    return true;
}

bool DeviceManager::saveHardwareProfile(const QString &profileName)
{
    Config::HardwareConfigManager manager;
    manager.config() = hardwareConfig_;
    if (!manager.saveProfile(profileName)) {
        setLastError(manager.lastError());
        return false;
    }
    hardwareProfileName_ = manager.currentProfileName();
    emit logMessage(QStringLiteral("已保存硬件 Profile: %1").arg(hardwareProfileName_));
    return true;
}

NFSScanner::Devices::Motion::SerialMotionController *DeviceManager::motionController()
{
    return serialMotion_;
}

NFSScanner::Devices::Motion::IMotionController *DeviceManager::motionInterface()
{
    return motionMockMode_ ? static_cast<Devices::Motion::IMotionController *>(mockMotion_)
                           : static_cast<Devices::Motion::IMotionController *>(serialMotion_);
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

NFSScanner::Devices::Probe::IProbeController *DeviceManager::probeController() const
{
    return probe_;
}

void DeviceManager::setMotionMockMode(bool enabled)
{
    if (motionMockMode_ == enabled) {
        return;
    }

    disconnectMotion();
    motionMockMode_ = enabled;
    updateMotionState();
    emit motionMockModeChanged(enabled);
    emit deviceStateChanged();
    emit logMessage(enabled ? QStringLiteral("运动平台切换为 Mock 模式。")
                            : QStringLiteral("运动平台切换为真实模式。"));
}

bool DeviceManager::motionMockMode() const
{
    return motionMockMode_;
}

bool DeviceManager::connectMotion()
{
    if (!hardwareConfig_.motion.enabled && !motionMockMode_) {
        setLastError(QStringLiteral("motion.enabled=false，未启用运动平台。"));
        return false;
    }

    if (motionMockMode_) {
        if (mockMotion_->isConnected()) {
            return true;
        }
        if (!mockMotion_->connectDevice()) {
            setLastError(QStringLiteral("Mock 运动平台连接失败。"));
            return false;
        }
        motionState_ = DeviceConnectionState::Mock;
        emit deviceStateChanged();
        emit logMessage(QStringLiteral("Mock 运动平台已连接。"));
        return true;
    }

    if (serialMotion_->isOpen()) {
        return true;
    }

    const QString port = hardwareConfig_.motion.port.trimmed();
    if (port.isEmpty()) {
        setLastError(QStringLiteral("未配置 motion.port。"));
        return false;
    }

    if (!serialMotion_->openPort(port, hardwareConfig_.motion.baudrate)) {
        setLastError(QStringLiteral("运动平台串口打开失败: %1").arg(port));
        motionState_ = DeviceConnectionState::Fault;
        emit deviceStateChanged();
        return false;
    }

    if (hardwareConfig_.motion.homeOnConnect) {
        emit logMessage(QStringLiteral("home_on_connect=true，发送 $H（需用户确认已启用该选项）。"));
        serialMotion_->home();
    }

    motionState_ = DeviceConnectionState::Connected;
    emit deviceStateChanged();
    emit logMessage(QStringLiteral("运动平台已连接: %1 @ %2")
                        .arg(port, QString::number(hardwareConfig_.motion.baudrate)));
    return true;
}

void DeviceManager::disconnectMotion()
{
    if (motionMockMode_) {
        mockMotion_->disconnectDevice();
    } else if (serialMotion_->isOpen()) {
        serialMotion_->closePort();
    }
    updateMotionState();
    emit deviceStateChanged();
}

bool DeviceManager::isMotionConnected() const
{
    return motionMockMode_ ? mockMotion_->isConnected() : serialMotion_->isOpen();
}

Devices::Spectrum::SpectrumConfig DeviceManager::spectrumConfigFromHardware() const
{
    Devices::Spectrum::SpectrumConfig config;
    config.startFreqHz = hardwareConfig_.spectrum.startFreqHz;
    config.stopFreqHz = hardwareConfig_.spectrum.stopFreqHz;
    config.centerFreqHz = (config.startFreqHz + config.stopFreqHz) * 0.5;
    config.spanHz = config.stopFreqHz - config.startFreqHz;
    config.sweepPoints = hardwareConfig_.spectrum.points;
    config.rbwHz = hardwareConfig_.spectrum.rbwHz;
    config.vbwHz = hardwareConfig_.spectrum.vbwHz;
    config.sweepTimeSec = hardwareConfig_.spectrum.sweepTimeS;
    config.traceId = hardwareConfig_.spectrum.trace;
    return config;
}

bool DeviceManager::ensureSpectrumAnalyzerFromConfig()
{
    QString analyzerName;
    const QString type = hardwareConfig_.spectrum.type.trimmed().toLower();
    if (type == QStringLiteral("mock")) {
        analyzerName = QStringLiteral("Mock Spectrum");
    } else if (type == QStringLiteral("zna67")) {
        analyzerName = QStringLiteral("R&S ZNA67");
    } else if (type == QStringLiteral("fsw")) {
        analyzerName = QStringLiteral("R&S FSW");
    } else if (type == QStringLiteral("n9020a")) {
        analyzerName = QStringLiteral("Keysight N9020A");
    } else {
        analyzerName = QStringLiteral("Generic SCPI");
    }

    if (spectrumAnalyzer_ && spectrumAnalyzer_->name() == analyzerName) {
        return true;
    }
    return createSpectrumAnalyzer(analyzerName);
}

bool DeviceManager::connectSpectrum()
{
    if (!hardwareConfig_.spectrum.enabled) {
        setLastError(QStringLiteral("spectrum.enabled=false，未启用频谱仪。"));
        return false;
    }

    if (!ensureSpectrumAnalyzerFromConfig()) {
        return false;
    }

    if (hardwareConfig_.spectrum.type.compare(QStringLiteral("mock"), Qt::CaseInsensitive) == 0) {
        QVariantMap options;
        options.insert(QStringLiteral("mock"), true);
        if (!connectSpectrumAnalyzer(options)) {
            return false;
        }
        spectrumState_ = DeviceConnectionState::Mock;
        emit deviceStateChanged();
        return true;
    }

    QVariantMap options;
    options.insert(QStringLiteral("host"), hardwareConfig_.spectrum.address);
    options.insert(QStringLiteral("port"), hardwareConfig_.spectrum.port);
    options.insert(QStringLiteral("timeout_ms"), hardwareConfig_.spectrum.timeoutMs);
    options.insert(QStringLiteral("retry_count"), hardwareConfig_.spectrum.retryCount);

    if (!connectSpectrumAnalyzer(options)) {
        spectrumState_ = DeviceConnectionState::Fault;
        emit deviceStateChanged();
        return false;
    }

    configureSpectrum(spectrumConfigFromHardware());
    spectrumState_ = DeviceConnectionState::Connected;
    emit deviceStateChanged();
    return true;
}

void DeviceManager::disconnectSpectrum()
{
    disconnectSpectrumAnalyzer();
    updateSpectrumState();
}

bool DeviceManager::isSpectrumConnected() const
{
    return spectrumAnalyzer_ && spectrumAnalyzer_->isConnected();
}

bool DeviceManager::connectCamera(bool requireEnabled)
{
    if (requireEnabled && !hardwareConfig_.camera.enabled) {
        setLastError(QStringLiteral("camera.enabled=false，未启用相机。"));
        return false;
    }

    releaseCamera();
    camera_ = Devices::Camera::createCamera(hardwareConfig_.camera.type, this);
    if (!camera_) {
        setLastError(QStringLiteral("无法创建相机实例。"));
        return false;
    }

    connect(camera_, &Devices::Camera::ICamera::logMessage, this, &DeviceManager::logMessage);
    connect(camera_, &Devices::Camera::ICamera::connectedChanged, this, [this](bool connected) {
        const bool mockType = hardwareConfig_.camera.type.compare(QStringLiteral("mock"), Qt::CaseInsensitive) == 0;
        cameraState_ = connected ? (mockType ? DeviceConnectionState::Mock : DeviceConnectionState::Connected)
                                 : DeviceConnectionState::Disconnected;
        emit deviceStateChanged();
    });

    QVariantMap options;
    options.insert(QStringLiteral("device_index"), hardwareConfig_.camera.deviceIndex);
    options.insert(QStringLiteral("timeout_ms"), hardwareConfig_.camera.timeoutMs);
    options.insert(QStringLiteral("save_dir"), hardwareConfig_.camera.saveDir);

    if (!camera_->connectDevice(options)) {
        setLastError(camera_->lastError());
        camera_->deleteLater();
        camera_ = nullptr;
        cameraState_ = DeviceConnectionState::Fault;
        emit deviceStateChanged();
        return false;
    }

    const bool mockType = hardwareConfig_.camera.type.compare(QStringLiteral("mock"), Qt::CaseInsensitive) == 0;
    cameraState_ = mockType ? DeviceConnectionState::Mock : DeviceConnectionState::Connected;
    emit deviceStateChanged();
    return true;
}

void DeviceManager::disconnectCamera()
{
    releaseCamera();
    cameraState_ = DeviceConnectionState::Disconnected;
    emit deviceStateChanged();
}

bool DeviceManager::isCameraConnected() const
{
    return camera_ && camera_->isConnected();
}

bool DeviceManager::connectProbe()
{
    if (!hardwareConfig_.probe.enabled) {
        setLastError(QStringLiteral("probe.enabled=false，未启用探头控制器。"));
        return false;
    }

    releaseProbeController();
    probe_ = Devices::Probe::createProbeController(hardwareConfig_.probe.type, this);
    connect(probe_, &Devices::Probe::IProbeController::logMessage, this, &DeviceManager::logMessage);
    connect(probe_, &Devices::Probe::IProbeController::connectedChanged, this, [this](bool connected) {
        const bool mockType = hardwareConfig_.probe.type.compare(QStringLiteral("mock"), Qt::CaseInsensitive) == 0;
        probeState_ = connected ? (mockType ? DeviceConnectionState::Mock : DeviceConnectionState::Connected)
                                : DeviceConnectionState::Disconnected;
        emit deviceStateChanged();
    });

    if (!probe_->connectDevice()) {
        setLastError(probe_->lastError());
        probe_->deleteLater();
        probe_ = nullptr;
        probeState_ = DeviceConnectionState::Fault;
        emit deviceStateChanged();
        return false;
    }

    const auto orientation = Devices::Probe::probeOrientationFromName(hardwareConfig_.probe.orientation);
    probe_->setOrientation(orientation);

    const bool mockType = hardwareConfig_.probe.type.compare(QStringLiteral("mock"), Qt::CaseInsensitive) == 0;
    probeState_ = mockType ? DeviceConnectionState::Mock : DeviceConnectionState::Connected;
    emit deviceStateChanged();
    return true;
}

void DeviceManager::disconnectProbe()
{
    releaseProbeController();
    probeState_ = DeviceConnectionState::Disconnected;
    emit deviceStateChanged();
}

bool DeviceManager::isProbeConnected() const
{
    return probe_ && probe_->isConnected();
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
    updateSpectrumState();
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
    updateSpectrumState();
    return true;
}

void DeviceManager::disconnectSpectrumAnalyzer()
{
    if (!spectrumDeviceHost_) {
        return;
    }
    QMetaObject::invokeMethod(spectrumDeviceHost_, "disconnectDevice", Qt::BlockingQueuedConnection);
    updateSpectrumState();
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

void DeviceManager::refreshDevices()
{
    emit logMessage(QStringLiteral("设备刷新：串口列表请在设备页点击「刷新串口」。"));
    emit deviceStateChanged();
}

bool DeviceManager::connectAll()
{
    bool ok = true;
    if (hardwareConfig_.motion.enabled) {
        ok = connectMotion() && ok;
    }
    if (hardwareConfig_.spectrum.enabled) {
        ok = connectSpectrum() && ok;
    }
    if (hardwareConfig_.camera.enabled) {
        ok = connectCamera() && ok;
    }
    if (hardwareConfig_.probe.enabled) {
        ok = connectProbe() && ok;
    }
    emit logMessage(QStringLiteral("连接全部完成。"));
    return ok;
}

void DeviceManager::disconnectAll()
{
    disconnectMotion();
    disconnectSpectrum();
    disconnectCamera();
    disconnectProbe();
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

DeviceConnectionState DeviceManager::probeState() const
{
    return probeState_;
}

DeviceHealth DeviceManager::runHealthCheck()
{
    DeviceHealth health;

    auto add = [&health](const QString &name, bool ok, const QString &message) {
        health.items.append(DeviceHealthItem{name, ok, message});
    };

    add(QStringLiteral("hardware_config"),
        QFile::exists(hardwareConfigPath_),
        QFile::exists(hardwareConfigPath_) ? hardwareConfigPath_ : QStringLiteral("配置文件缺失"));

    if (hardwareConfig_.motion.enabled) {
        add(QStringLiteral("motion"),
            isMotionConnected(),
            isMotionConnected() ? QStringLiteral("运动平台就绪") : QStringLiteral("运动平台未连接"));
    } else {
        add(QStringLiteral("motion"), true, QStringLiteral("motion 未启用"));
    }

    if (hardwareConfig_.spectrum.enabled) {
        add(QStringLiteral("spectrum"),
            isSpectrumConnected(),
            isSpectrumConnected() ? QStringLiteral("频谱仪就绪") : QStringLiteral("频谱仪未连接"));
    } else {
        add(QStringLiteral("spectrum"), true, QStringLiteral("spectrum 未启用"));
    }

    if (hardwareConfig_.camera.enabled) {
        add(QStringLiteral("camera"),
            isCameraConnected(),
            isCameraConnected() ? QStringLiteral("相机就绪") : QStringLiteral("相机未连接"));
    } else {
        add(QStringLiteral("camera"), true, QStringLiteral("camera 未启用"));
    }

    if (hardwareConfig_.probe.enabled) {
        add(QStringLiteral("probe"),
            isProbeConnected(),
            isProbeConnected() ? QStringLiteral("探头控制器就绪") : QStringLiteral("探头未连接"));
    } else {
        add(QStringLiteral("probe"), true, QStringLiteral("probe 未启用"));
    }

    return health;
}

QString DeviceManager::lastError() const
{
    return lastError_;
}

void DeviceManager::setLastError(const QString &message)
{
    lastError_ = message;
    if (!message.isEmpty()) {
        Infra::Logger::appendGlobalHistory(Infra::Logger::format(Infra::Logger::Level::Error, message));
    }
}

void DeviceManager::updateMotionState()
{
    if (motionMockMode_) {
        motionState_ = mockMotion_->isConnected() ? DeviceConnectionState::Mock
                                                  : DeviceConnectionState::Disconnected;
    } else {
        motionState_ = serialMotion_->isOpen() ? DeviceConnectionState::Connected
                                                : DeviceConnectionState::Disconnected;
    }
}

void DeviceManager::updateSpectrumState()
{
    if (!spectrumAnalyzer_) {
        spectrumState_ = DeviceConnectionState::Disconnected;
        return;
    }
    if (!spectrumAnalyzer_->isConnected()) {
        spectrumState_ = DeviceConnectionState::Disconnected;
        return;
    }
    if (hardwareConfig_.spectrum.type.compare(QStringLiteral("mock"), Qt::CaseInsensitive) == 0) {
        spectrumState_ = DeviceConnectionState::Mock;
    } else {
        spectrumState_ = DeviceConnectionState::Connected;
    }
}

void DeviceManager::releaseProbeController()
{
    if (!probe_) {
        return;
    }
    probe_->disconnectDevice();
    probe_->deleteLater();
    probe_ = nullptr;
}

void DeviceManager::releaseCamera()
{
    if (!camera_) {
        return;
    }
    camera_->disconnectDevice();
    camera_->deleteLater();
    camera_ = nullptr;
}

} // namespace NFSScanner::Core
