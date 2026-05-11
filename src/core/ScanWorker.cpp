#include "core/ScanWorker.h"

#include <QMetaType>
#include <QRandomGenerator>
#include <QtMath>

#include <algorithm>

namespace NFSScanner::Core {

ScanWorker::ScanWorker(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<NFSScanner::Core::ScanPoint>("NFSScanner::Core::ScanPoint");
    timer_.setInterval(100);
    timer_.setSingleShot(false);
    connect(&timer_, &QTimer::timeout, this, &ScanWorker::advanceMockPoint);
}

void ScanWorker::setConfig(const ScanConfig &config)
{
    if (running_) {
        emit logMessage(QStringLiteral("扫描运行中，新的扫描参数将在下次扫描生效。"));
        return;
    }

    config_ = config;
}

const ScanConfig &ScanWorker::config() const
{
    return config_;
}

bool ScanWorker::isRunning() const
{
    return running_;
}

void ScanWorker::startMockScan()
{
    if (running_) {
        emit logMessage(QStringLiteral("扫描已经在运行。"));
        return;
    }

    const int total = config_.totalPoints();
    if (total <= 0) {
        emit logMessage(QStringLiteral("扫描参数无效：扫描点数量为 0。"));
        emit scanFinished(false);
        return;
    }

    currentIndex_ = 0;
    running_ = true;
    timer_.start();

    emit logMessage(QStringLiteral("Mock 扫描启动：%1 x %2，共 %3 点。")
                        .arg(config_.columns)
                        .arg(config_.rows)
                        .arg(total));
}

void ScanWorker::stopScan()
{
    if (!running_) {
        return;
    }

    finishScan(false);
}

void ScanWorker::advanceMockPoint()
{
    if (!running_) {
        return;
    }

    const int total = config_.totalPoints();
    if (currentIndex_ >= total) {
        finishScan(true);
        return;
    }

    const ScanPoint point = createPoint(currentIndex_);
    ++currentIndex_;

    emit progressChanged(currentIndex_, total, point);
    emit logMessage(QStringLiteral("扫描进度 %1/%2：X=%3 mm, Y=%4 mm, F=%5 MHz, P=%6 dBm")
                        .arg(currentIndex_)
                        .arg(total)
                        .arg(point.xMm, 0, 'f', 2)
                        .arg(point.yMm, 0, 'f', 2)
                        .arg(point.frequencyMhz, 0, 'f', 2)
                        .arg(point.powerDbm, 0, 'f', 2));

    if (currentIndex_ >= total) {
        finishScan(true);
    }
}

ScanPoint ScanWorker::createPoint(int index) const
{
    const int safeColumns = std::max(1, config_.columns);
    const int total = std::max(1, config_.totalPoints());
    const int xIndex = index % safeColumns;
    const int yIndex = index / safeColumns;

    const double x = static_cast<double>(xIndex) * config_.stepMm;
    const double y = static_cast<double>(yIndex) * config_.stepMm;
    const double ratio = total > 1 ? static_cast<double>(index) / static_cast<double>(total - 1) : 0.0;
    const double noise = QRandomGenerator::global()->generateDouble() * 0.08 - 0.04;
    const double field = 0.5
        + 0.28 * qSin(static_cast<double>(xIndex) * 0.33)
        + 0.18 * qCos(static_cast<double>(yIndex) * 0.41)
        + 0.10 * qSin(static_cast<double>(xIndex + yIndex) * 0.17)
        + noise;
    const double normalized = std::clamp(field, 0.0, 1.0);

    ScanPoint point;
    point.index = index;
    point.xIndex = xIndex;
    point.yIndex = yIndex;
    point.xMm = x;
    point.yMm = y;
    point.frequencyMhz = config_.startFrequencyMhz
        + ratio * (config_.stopFrequencyMhz - config_.startFrequencyMhz);
    point.powerDbm = -85.0 + normalized * 55.0;
    point.normalizedLevel = normalized;
    return point;
}

void ScanWorker::finishScan(bool completed)
{
    timer_.stop();
    running_ = false;

    emit logMessage(completed
                        ? QStringLiteral("Mock 扫描完成。")
                        : QStringLiteral("Mock 扫描已停止。"));
    emit scanFinished(completed);
}

} // namespace NFSScanner::Core
