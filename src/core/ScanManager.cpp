#include "core/ScanManager.h"

#include "core/ScanPathPlanner.h"
#include "core/ScanResult.h"
#include "devices/motion/IMotionController.h"
#include "devices/motion/SerialMotionController.h"

#include <QDateTime>
#include <QMetaType>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <utility>

namespace NFSScanner::Core {

namespace Spectrum = NFSScanner::Devices::Spectrum;

QString scanStateToString(ScanState state)
{
    switch (state) {
    case ScanState::Idle:
        return QStringLiteral("就绪");
    case ScanState::Preparing:
        return QStringLiteral("准备中");
    case ScanState::Running:
        return QStringLiteral("扫描中");
    case ScanState::Paused:
        return QStringLiteral("已暂停");
    case ScanState::Stopping:
        return QStringLiteral("正在停止");
    case ScanState::Stopped:
        return QStringLiteral("已停止");
    case ScanState::Finished:
        return QStringLiteral("已完成");
    case ScanState::Error:
        return QStringLiteral("错误");
    }

    return QStringLiteral("未知");
}

ScanManager::ScanManager(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<Spectrum::SpectrumAcquisitionRequest>("SpectrumAcquisitionRequest");
    qRegisterMetaType<Spectrum::SpectrumAcquisitionResult>("SpectrumAcquisitionResult");

    timer_.setSingleShot(true);
    connect(&timer_, &QTimer::timeout, this, [this]() {
        if ((state_ == ScanState::Running || state_ == ScanState::Paused) && currentIndex_ < points_.size()) {
            requestAcquisition(points_.at(currentIndex_));
        }
    });
    motionPollTimer_.setInterval(150);
    connect(&motionPollTimer_, &QTimer::timeout, this, &ScanManager::pollMotion);
    connect(&fallbackSpectrum_, &Spectrum::ISpectrumAnalyzer::logMessage, this, &ScanManager::logMessage);
    connect(&fallbackSpectrum_, &Spectrum::ISpectrumAnalyzer::errorOccurred, this, &ScanManager::logMessage);
}

ScanManager::~ScanManager()
{
    timer_.stop();
    motionPollTimer_.stop();
    shutdownAcquisitionThread(true);
}

void ScanManager::startScan(const ScanConfig &config)
{
    if (state_ == ScanState::Running || state_ == ScanState::Paused) {
        emit logMessage(QStringLiteral("扫描正在进行中，请先停止当前扫描。"));
        return;
    }

    shutdownAcquisitionThread(true);
    setState(ScanState::Preparing);

    ScanPathPlanner planner;
    QVector<ScanPoint> points = planner.generate(config);
    if (points.isEmpty()) {
        const QString message = planner.lastError().isEmpty()
            ? QStringLiteral("扫描路径生成失败。")
            : planner.lastError();
        points_.clear();
        currentIndex_ = 0;
        setState(ScanState::Error);
        emit scanError(message);
        return;
    }

    config_ = config;
    points_ = std::move(points);
    currentIndex_ = 0;
    acquisitionInProgress_ = false;
    motionInProgress_ = false;
    motionPollTimer_.stop();

    fallbackSpectrum_.configure(spectrumConfig_);
    fallbackSpectrum_.connectDevice(QVariantMap{});

    if (analyzer_ && analyzer_->isConnected() && !analyzer_->configure(spectrumConfig_)) {
        const QString message = QStringLiteral("配置频谱仪失败：%1").arg(analyzer_->lastError());
        points_.clear();
        setState(ScanState::Error);
        emit scanError(message);
        return;
    }

    if (!storage_.beginTask(config_, points_.size())) {
        const QString message = QStringLiteral("创建任务目录失败：%1").arg(storage_.lastError());
        points_.clear();
        currentIndex_ = 0;
        setState(ScanState::Error);
        emit scanError(message);
        return;
    }

    setupAcquisitionThread();

    const int intervalMs = std::max(50, config_.dwellMs);
    timer_.setInterval(intervalMs);

    emit taskDirChanged(storage_.taskDir());
    emit logMessage(QStringLiteral("任务目录：%1").arg(storage_.taskDir()));
    emit progressChanged(0, points_.size());
    emit estimatedChanged(points_.size(), static_cast<int>(std::ceil(points_.size() * intervalMs / 1000.0)));
    emit logMessage(QStringLiteral("扫描开始，共 %1 个点，驻留时间 %2 ms，蛇形扫描：%3，采集超时 %4 ms，重试 %5 次。")
                        .arg(points_.size())
                        .arg(intervalMs)
                        .arg(config_.snakeMode ? QStringLiteral("启用") : QStringLiteral("关闭"))
                        .arg(acquisitionOptions_.timeoutMs)
                        .arg(acquisitionOptions_.retryCount));

    if (!analyzer_ || !analyzer_->isConnected()) {
        emit logMessage(QStringLiteral("真实频谱仪未连接，将使用 Mock Spectrum 数据。"));
    }

    setState(ScanState::Running);
    processNextPoint();
}

void ScanManager::pauseScan()
{
    if (state_ != ScanState::Running) {
        return;
    }

    timer_.stop();
    setState(ScanState::Paused);
    emit logMessage(acquisitionInProgress_
                        ? QStringLiteral("扫描已暂停，当前采集完成后生效。")
                        : QStringLiteral("扫描已暂停。"));
}

void ScanManager::resumeScan()
{
    if (state_ != ScanState::Paused) {
        return;
    }

    setState(ScanState::Running);
    emit logMessage(QStringLiteral("扫描继续。"));
    if (!acquisitionInProgress_) {
        processNextPoint();
    }
}

void ScanManager::stopScan()
{
    if (state_ != ScanState::Running && state_ != ScanState::Paused) {
        return;
    }

    timer_.stop();
    motionPollTimer_.stop();
    motionInProgress_ = false;
    setState(ScanState::Stopping);
    storage_.finishTask();
    setState(ScanState::Stopped);
    emit logMessage(QStringLiteral("扫描已停止，已保存部分数据：%1").arg(storage_.taskDir()));

    if (!acquisitionInProgress_) {
        shutdownAcquisitionThread(true);
    }
}

void ScanManager::setSpectrumAnalyzer(Spectrum::ISpectrumAnalyzer *analyzer)
{
    analyzer_ = analyzer;
}

void ScanManager::setSpectrumConfig(const Spectrum::SpectrumConfig &config)
{
    spectrumConfig_ = config;
}

void ScanManager::setAcquisitionOptions(const ScanAcquisitionOptions &options)
{
    acquisitionOptions_.timeoutMs = std::clamp(options.timeoutMs, 1000, 60000);
    acquisitionOptions_.retryCount = std::clamp(options.retryCount, 0, 5);
    acquisitionOptions_.stopOnError = options.stopOnError;
}

void ScanManager::setMotionController(NFSScanner::Devices::Motion::IMotionController *motionController)
{
    if (motionController_ == motionController) {
        return;
    }

    if (motionController_) {
        disconnect(motionController_, nullptr, this, nullptr);
    }

    motionController_ = motionController;
    if (!motionController_) {
        return;
    }

    connect(motionController_, &NFSScanner::Devices::Motion::IMotionController::positionChanged,
            this, [this](double x, double y, double z) {
                motionCurrentX_ = x;
                motionCurrentY_ = y;
                motionCurrentZ_ = z;
            });

    if (auto *serial = qobject_cast<NFSScanner::Devices::Motion::SerialMotionController *>(motionController_)) {
        connect(serial, &NFSScanner::Devices::Motion::SerialMotionController::statusChanged,
                this, [this](const QString &status) {
                    motionStatus_ = status;
                });
    }
}

void ScanManager::setUseRealMotion(bool enabled)
{
    useRealMotion_ = enabled;
}

ScanState ScanManager::state() const
{
    return state_;
}

int ScanManager::currentIndex() const
{
    return currentIndex_;
}

int ScanManager::totalCount() const
{
    return points_.size();
}

void ScanManager::processNextPoint()
{
    if (state_ != ScanState::Running || acquisitionInProgress_ || motionInProgress_) {
        return;
    }

    if (currentIndex_ >= points_.size()) {
        finishScan();
        return;
    }

    const ScanPoint point = points_.at(currentIndex_);
    if (useRealMotion_) {
        beginPointMotion(point);
    } else {
        startDwellForPoint(point);
    }
}

void ScanManager::beginPointMotion(const ScanPoint &point)
{
    if (!motionController_ || !motionController_->isConnected()) {
        const QString message = QStringLiteral("Real motion is enabled, but motion controller is not connected.");
        timer_.stop();
        setState(ScanState::Error);
        emit scanError(message);
        return;
    }

    activePoint_ = point;
    emit logMessage(QStringLiteral("移动到扫描点 %1/%2：X=%3 Y=%4 Z=%5")
                        .arg(point.index)
                        .arg(points_.size())
                        .arg(point.x, 0, 'f', 3)
                        .arg(point.y, 0, 'f', 3)
                        .arg(point.z, 0, 'f', 3));

    bool moveOk = false;
    if (auto *serial = qobject_cast<NFSScanner::Devices::Motion::SerialMotionController *>(motionController_)) {
        moveOk = serial->moveAbs(point.x, point.y, point.z, config_.feed);
    } else {
        moveOk = motionController_->moveTo(point.x, point.y, point.z);
    }

    if (!moveOk) {
        const QString message = QStringLiteral("Failed to send motion command for point %1.").arg(point.index);
        setState(ScanState::Error);
        emit scanError(message);
        return;
    }

    motionInProgress_ = true;
    motionElapsedTimer_.restart();
    motionPollTimer_.start();
}

void ScanManager::pollMotion()
{
    if (!motionInProgress_) {
        motionPollTimer_.stop();
        return;
    }

    if (state_ != ScanState::Running && state_ != ScanState::Paused) {
        motionPollTimer_.stop();
        motionInProgress_ = false;
        return;
    }

    if (auto *serial = qobject_cast<NFSScanner::Devices::Motion::SerialMotionController *>(motionController_)) {
        serial->queryPosition();
        const auto position = serial->currentPosition();
        motionCurrentX_ = position.x;
        motionCurrentY_ = position.y;
        motionCurrentZ_ = position.z;
        motionStatus_ = serial->currentStatus();
    }

    if (motionTargetReached(activePoint_)) {
        motionPollTimer_.stop();
        motionInProgress_ = false;
        emit logMessage(QStringLiteral("运动到位：X=%1 Y=%2 Z=%3")
                            .arg(motionCurrentX_, 0, 'f', 3)
                            .arg(motionCurrentY_, 0, 'f', 3)
                            .arg(motionCurrentZ_, 0, 'f', 3));
        startDwellForPoint(activePoint_);
        return;
    }

    if (motionElapsedTimer_.elapsed() >= motionTimeoutMs_) {
        motionPollTimer_.stop();
        motionInProgress_ = false;
        const QString message = QStringLiteral("运动超时：target X=%1 Y=%2 Z=%3 current X=%4 Y=%5 Z=%6 status=%7")
                                    .arg(activePoint_.x, 0, 'f', 3)
                                    .arg(activePoint_.y, 0, 'f', 3)
                                    .arg(activePoint_.z, 0, 'f', 3)
                                    .arg(motionCurrentX_, 0, 'f', 3)
                                    .arg(motionCurrentY_, 0, 'f', 3)
                                    .arg(motionCurrentZ_, 0, 'f', 3)
                                    .arg(motionStatus_);
        setState(ScanState::Error);
        emit scanError(message);
    }
}

bool ScanManager::motionTargetReached(const ScanPoint &point) const
{
    const bool positionOk = std::abs(motionCurrentX_ - point.x) <= motionToleranceMm_
        && std::abs(motionCurrentY_ - point.y) <= motionToleranceMm_
        && std::abs(motionCurrentZ_ - point.z) <= motionToleranceMm_;
    if (!positionOk) {
        return false;
    }

    if (qobject_cast<NFSScanner::Devices::Motion::SerialMotionController *>(motionController_)) {
        return motionStatus_.contains(QStringLiteral("Idle"), Qt::CaseInsensitive);
    }
    return true;
}

void ScanManager::startDwellForPoint(const ScanPoint &point)
{
    const int total = points_.size();
    const int intervalMs = std::max(50, config_.dwellMs);
    const int remaining = std::max(0, total - currentIndex_);
    const int estimatedSeconds = static_cast<int>(std::ceil(remaining * intervalMs / 1000.0));

    emit currentPointChanged(point.index, total, point.x, point.y, point.z);
    emit estimatedChanged(remaining, estimatedSeconds);
    emit logMessage(QStringLiteral("到达扫描点 %1/%2：X=%3 Y=%4 Z=%5，等待采集。")
                        .arg(point.index)
                        .arg(total)
                        .arg(point.x, 0, 'f', 2)
                        .arg(point.y, 0, 'f', 2)
                        .arg(point.z, 0, 'f', 2));

    timer_.start(intervalMs);
}

void ScanManager::requestAcquisition(const ScanPoint &point)
{
    if ((state_ != ScanState::Running && state_ != ScanState::Paused) || !acquisitionWorker_) {
        return;
    }

    Spectrum::SpectrumAcquisitionRequest request;
    request.pointIndex = point.index;
    request.x = point.x;
    request.y = point.y;
    request.z = point.z;
    request.timeoutMs = acquisitionOptions_.timeoutMs;
    request.retryCount = acquisitionOptions_.retryCount;
    acquisitionInProgress_ = true;
    emit acquireSpectrumRequested(request);
}

void ScanManager::onAcquisitionFinished(const Spectrum::SpectrumAcquisitionResult &result)
{
    acquisitionInProgress_ = false;

    if (state_ == ScanState::Stopped || state_ == ScanState::Stopping || state_ == ScanState::Idle) {
        emit logMessage(QStringLiteral("收到已停止扫描的采集结果，已忽略：点 %1。").arg(result.pointIndex));
        shutdownAcquisitionThread(false);
        return;
    }

    if (state_ != ScanState::Running && state_ != ScanState::Paused) {
        return;
    }

    if (!result.ok) {
        const QString message = QStringLiteral("频谱采集失败：点 %1，%2").arg(result.pointIndex).arg(result.error);
        emit logMessage(message);
        if (acquisitionOptions_.stopOnError) {
            timer_.stop();
            storage_.finishTask();
            setState(ScanState::Error);
            shutdownAcquisitionThread(false);
            emit scanError(message);
            return;
        }

        ++currentIndex_;
        emit progressChanged(currentIndex_, points_.size());
        if (state_ == ScanState::Running) {
            processNextPoint();
        }
        return;
    }

    ScanPoint point;
    point.index = result.pointIndex;
    point.x = result.x;
    point.y = result.y;
    point.z = result.z;

    ScanResult scanResult;
    scanResult.index = result.pointIndex;
    scanResult.x = result.x;
    scanResult.y = result.y;
    scanResult.z = result.z;
    scanResult.timestamp = result.timestamp.isValid() ? result.timestamp : QDateTime::currentDateTime();
    scanResult.traceId = result.trace.traceId;
    scanResult.freqs = result.trace.freqs;
    scanResult.values = result.trace.values;
    scanResult.trace = result.trace;

    if (!storage_.appendPoint(point, scanResult.timestamp) || !storage_.appendTrace(scanResult)) {
        timer_.stop();
        storage_.finishTask();
        setState(ScanState::Error);
        shutdownAcquisitionThread(false);
        emit scanError(QStringLiteral("数据保存失败：%1").arg(storage_.lastError()));
        return;
    }

    ++currentIndex_;
    const int total = points_.size();
    const int remaining = std::max(0, total - currentIndex_);
    const int estimatedSeconds = static_cast<int>(std::ceil(remaining * timer_.interval() / 1000.0));

    emit progressChanged(currentIndex_, total);
    emit estimatedChanged(remaining, estimatedSeconds);
    emit logMessage(QStringLiteral("扫描点 %1/%2 采集完成：X=%3 Y=%4 Z=%5，频率点=%6。")
                        .arg(currentIndex_)
                        .arg(total)
                        .arg(result.x, 0, 'f', 2)
                        .arg(result.y, 0, 'f', 2)
                        .arg(result.z, 0, 'f', 2)
                        .arg(result.trace.values.size()));

    if (currentIndex_ >= total) {
        finishScan();
    } else if (state_ == ScanState::Running) {
        processNextPoint();
    } else {
        emit logMessage(QStringLiteral("扫描已暂停，等待继续。"));
    }
}

void ScanManager::setupAcquisitionThread()
{
    shutdownAcquisitionThread(true);

    acquisitionThread_ = new QThread(this);
    acquisitionWorker_ = new Spectrum::SpectrumAcquisitionWorker;
    acquisitionWorker_->setAnalyzer(analyzer_);
    acquisitionWorker_->setFallbackAnalyzer(&fallbackSpectrum_);
    acquisitionWorker_->setStopOnError(acquisitionOptions_.stopOnError);
    acquisitionWorker_->setRetryCount(acquisitionOptions_.retryCount);
    acquisitionWorker_->moveToThread(acquisitionThread_);

    connect(acquisitionThread_, &QThread::finished, acquisitionWorker_, &QObject::deleteLater);
    connect(this, &ScanManager::acquireSpectrumRequested,
            acquisitionWorker_, &Spectrum::SpectrumAcquisitionWorker::acquire,
            Qt::QueuedConnection);
    connect(acquisitionWorker_, &Spectrum::SpectrumAcquisitionWorker::acquisitionStarted,
            this, [this](int pointIndex) {
                emit logMessage(QStringLiteral("开始异步频谱采集：点 %1。").arg(pointIndex));
            });
    connect(acquisitionWorker_, &Spectrum::SpectrumAcquisitionWorker::acquisitionFinished,
            this, &ScanManager::onAcquisitionFinished,
            Qt::QueuedConnection);
    connect(acquisitionWorker_, &Spectrum::SpectrumAcquisitionWorker::logMessage,
            this, &ScanManager::logMessage,
            Qt::QueuedConnection);
    connect(acquisitionWorker_, &Spectrum::SpectrumAcquisitionWorker::errorOccurred,
            this, &ScanManager::logMessage,
            Qt::QueuedConnection);

    acquisitionThread_->start();
}

void ScanManager::shutdownAcquisitionThread(bool waitForFinish)
{
    if (!acquisitionThread_) {
        acquisitionWorker_ = nullptr;
        return;
    }

    acquisitionThread_->quit();
    if (waitForFinish) {
        if (!acquisitionThread_->wait(70000)) {
            acquisitionThread_->terminate();
            acquisitionThread_->wait(3000);
        }
    } else if (!acquisitionThread_->wait(10)) {
        return;
    }

    acquisitionThread_->deleteLater();
    acquisitionThread_ = nullptr;
    acquisitionWorker_ = nullptr;
}

void ScanManager::finishScan()
{
    timer_.stop();
    motionPollTimer_.stop();
    motionInProgress_ = false;
    if (!storage_.finishTask()) {
        setState(ScanState::Error);
        shutdownAcquisitionThread(false);
        emit scanError(QStringLiteral("完成任务保存失败：%1").arg(storage_.lastError()));
        return;
    }
    setState(ScanState::Finished);
    shutdownAcquisitionThread(false);
    emit scanFinished(storage_.taskDir());
}

void ScanManager::setState(ScanState state)
{
    if (state_ == state) {
        return;
    }

    state_ = state;
    emit stateChanged(scanStateToString(state_));
}

} // namespace NFSScanner::Core
