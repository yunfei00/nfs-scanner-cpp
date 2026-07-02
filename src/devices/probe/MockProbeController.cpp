#include "devices/probe/MockProbeController.h"

#include <QThread>

namespace NFSScanner::Devices::Probe {

QString probeOrientationName(ProbeOrientation orientation)
{
    return orientation == ProbeOrientation::Hy ? QStringLiteral("Hy") : QStringLiteral("Hx");
}

ProbeOrientation probeOrientationFromName(const QString &name)
{
    return name.compare(QStringLiteral("Hy"), Qt::CaseInsensitive) == 0 ? ProbeOrientation::Hy
                                                                        : ProbeOrientation::Hx;
}

MockProbeController::MockProbeController(QObject *parent)
    : IProbeController(parent)
{
}

QString MockProbeController::name() const
{
    return QStringLiteral("Mock Probe");
}

bool MockProbeController::connectDevice()
{
    connected_ = true;
    lastError_.clear();
    emit logMessage(QStringLiteral("Mock Probe 已连接，当前方向 %1。").arg(probeOrientationName(orientation_)));
    emit connectedChanged(true);
    return true;
}

void MockProbeController::disconnectDevice()
{
    connected_ = false;
    emit logMessage(QStringLiteral("Mock Probe 已断开。"));
    emit connectedChanged(false);
}

bool MockProbeController::isConnected() const
{
    return connected_;
}

bool MockProbeController::setOrientation(ProbeOrientation orientation)
{
    if (!connected_) {
        lastError_ = QStringLiteral("探头控制器未连接。");
        emit errorOccurred(lastError_);
        return false;
    }

    if (orientation_ != orientation && switchDelayMs_ > 0) {
        QThread::msleep(static_cast<unsigned long>(switchDelayMs_));
    }

    orientation_ = orientation;
    lastError_.clear();
    emit logMessage(QStringLiteral("Mock Probe 方向切换为 %1。").arg(probeOrientationName(orientation_)));
    emit orientationChanged(orientation_);
    return true;
}

ProbeOrientation MockProbeController::currentOrientation() const
{
    return orientation_;
}

QString MockProbeController::lastError() const
{
    return lastError_;
}

} // namespace NFSScanner::Devices::Probe
