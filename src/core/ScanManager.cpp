#include "core/ScanManager.h"

#include "core/ScanResult.h"
#include "core/ScanPathPlanner.h"

#include <QDateTime>

#include <algorithm>
#include <cmath>
#include <utility>

namespace NFSScanner::Core {

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
    timer_.setSingleShot(false);
    connect(&timer_, &QTimer::timeout, this, &ScanManager::advanceScan);
}

ScanManager::~ScanManager()
{
    timer_.stop();
}

void ScanManager::startScan(const ScanConfig &config)
{
    if (state_ == ScanState::Running || state_ == ScanState::Paused) {
        emit logMessage(QStringLiteral("扫描正在进行中，请先停止当前扫描。"));
        return;
    }

    setState(ScanState::Preparing);

    ScanPathPlanner planner;
    QVector<ScanPoint> points = planner.generate(config);
    if (points.isEmpty()) {
        const QString message = planner.lastError().isEmpty()
            ? QStringLiteral("扫描路径生成失败。")
            : planner.lastError();
        timer_.stop();
        points_.clear();
        currentIndex_ = 0;
        setState(ScanState::Error);
        emit scanError(message);
        return;
    }

    config_ = config;
    points_ = std::move(points);
    currentIndex_ = 0;

    mockSpectrum_.connectDevice();
    if (!storage_.beginTask(config_, points_.size())) {
        const QString message = QStringLiteral("创建任务目录失败：%1").arg(storage_.lastError());
        timer_.stop();
        points_.clear();
        currentIndex_ = 0;
        setState(ScanState::Error);
        emit scanError(message);
        return;
    }

    const int intervalMs = std::max(50, config_.dwellMs);
    timer_.setInterval(intervalMs);

    emit taskDirChanged(storage_.taskDir());
    emit logMessage(QStringLiteral("任务目录：%1").arg(storage_.taskDir()));
    emit progressChanged(0, points_.size());
    emit estimatedChanged(points_.size(), static_cast<int>(std::ceil(points_.size() * intervalMs / 1000.0)));
    emit logMessage(QStringLiteral("扫描开始，共 %1 个点，驻留时间 %2 ms，蛇形扫描：%3。")
                        .arg(points_.size())
                        .arg(intervalMs)
                        .arg(config_.snakeMode ? QStringLiteral("启用") : QStringLiteral("关闭")));

    setState(ScanState::Running);
    timer_.start();
}

void ScanManager::pauseScan()
{
    if (state_ != ScanState::Running) {
        return;
    }

    timer_.stop();
    setState(ScanState::Paused);
    emit logMessage(QStringLiteral("扫描已暂停。"));
}

void ScanManager::resumeScan()
{
    if (state_ != ScanState::Paused) {
        return;
    }

    setState(ScanState::Running);
    timer_.start();
    emit logMessage(QStringLiteral("扫描继续。"));
}

void ScanManager::stopScan()
{
    if (state_ != ScanState::Running && state_ != ScanState::Paused) {
        return;
    }

    setState(ScanState::Stopping);
    timer_.stop();
    storage_.finishTask();
    setState(ScanState::Stopped);
    emit logMessage(QStringLiteral("扫描已停止，已保存部分数据：%1").arg(storage_.taskDir()));
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

void ScanManager::advanceScan()
{
    if (state_ != ScanState::Running) {
        return;
    }

    if (currentIndex_ >= points_.size()) {
        timer_.stop();
        if (!storage_.finishTask()) {
            setState(ScanState::Error);
            emit scanError(QStringLiteral("完成任务保存失败：%1").arg(storage_.lastError()));
            return;
        }
        setState(ScanState::Finished);
        emit scanFinished(storage_.taskDir());
        return;
    }

    const ScanPoint point = points_.at(currentIndex_);
    const QDateTime timestamp = QDateTime::currentDateTime();
    const NFSScanner::Devices::Spectrum::SpectrumTrace trace = mockSpectrum_.singleSweep(point.index, point.x, point.y, point.z);

    ScanResult result;
    result.index = point.index;
    result.x = point.x;
    result.y = point.y;
    result.z = point.z;
    result.timestamp = timestamp;
    result.traceId = trace.traceId;
    result.freqs = trace.freqs;
    result.values = trace.values;

    if (!storage_.appendPoint(point, timestamp) || !storage_.appendTrace(result)) {
        timer_.stop();
        setState(ScanState::Error);
        emit scanError(QStringLiteral("数据保存失败：%1").arg(storage_.lastError()));
        return;
    }

    ++currentIndex_;

    const int total = points_.size();
    const int remaining = std::max(0, total - currentIndex_);
    const int estimatedSeconds = static_cast<int>(std::ceil(remaining * timer_.interval() / 1000.0));

    emit currentPointChanged(currentIndex_, total, point.x, point.y, point.z);
    emit progressChanged(currentIndex_, total);
    emit estimatedChanged(remaining, estimatedSeconds);
    emit logMessage(QStringLiteral("扫描点 %1/%2：X=%3 Y=%4 Z=%5")
                        .arg(currentIndex_)
                        .arg(total)
                        .arg(point.x, 0, 'f', 2)
                        .arg(point.y, 0, 'f', 2)
                        .arg(point.z, 0, 'f', 2));

    if (currentIndex_ >= total) {
        timer_.stop();
        if (!storage_.finishTask()) {
            setState(ScanState::Error);
            emit scanError(QStringLiteral("完成任务保存失败：%1").arg(storage_.lastError()));
            return;
        }
        setState(ScanState::Finished);
        emit scanFinished(storage_.taskDir());
    }
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
