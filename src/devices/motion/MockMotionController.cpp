#include "devices/motion/MockMotionController.h"

#include "devices/FaultInjectionConfig.h"
#include "diagnostics/HardwareSessionRecorder.h"

#include <QThread>

namespace NFSScanner::Devices::Motion {

MockMotionController::MockMotionController(QObject *parent)
    : IMotionController(parent)
{
}

QString MockMotionController::name() const
{
    return QStringLiteral("Mock Motion Controller");
}

bool MockMotionController::connectDevice()
{
    FaultInjectionConfig &fault = globalFaultInjectionConfig();
    if (fault.enabled && fault.connectFail) {
        emit errorOccurred(QStringLiteral("Fault injection: motion connect_fail"));
        Diagnostics::HardwareSessionRecorder::recordGrbl(QStringLiteral("event"), QStringLiteral("connect"), QStringLiteral("fail"), false);
        return false;
    }

    if (connected_) {
        return true;
    }

    if (fault.enabled && (fault.timeout || FaultInjectionConfig::shouldRandomTimeout(fault))) {
        QThread::msleep(50);
        emit errorOccurred(QStringLiteral("Fault injection: motion timeout"));
        return false;
    }

    connected_ = true;
    emit connectionChanged(true);
    emit positionChanged(xMm_, yMm_, zMm_);
    Diagnostics::HardwareSessionRecorder::recordGrbl(QStringLiteral("event"), QStringLiteral("connect"), QStringLiteral("ok"), true);
    return true;
}

void MockMotionController::disconnectDevice()
{
    if (!connected_) {
        return;
    }

    connected_ = false;
    emit connectionChanged(false);
}

bool MockMotionController::isConnected() const
{
    return connected_;
}

bool MockMotionController::moveTo(double xMm, double yMm, double zMm)
{
    FaultInjectionConfig &fault = globalFaultInjectionConfig();
    fault.onCommandSent();

    if (!connected_) {
        emit errorOccurred(QStringLiteral("运动控制器未连接，无法移动。"));
        return false;
    }

    if (fault.enabled && fault.motionAlarm) {
        emit errorOccurred(QStringLiteral("Fault injection: motion_alarm"));
        Diagnostics::HardwareSessionRecorder::recordGrbl(QStringLiteral("rx"), QStringLiteral("G1"), QStringLiteral("<Alarm|...>"), false);
        return false;
    }

    if (fault.enabled && fault.limitError) {
        emit errorOccurred(QStringLiteral("Fault injection: limit_error"));
        return false;
    }

    if (fault.enabled && fault.disconnectAfterNCommands > 0
        && fault.commandCount() >= fault.disconnectAfterNCommands) {
        disconnectDevice();
        emit errorOccurred(QStringLiteral("Fault injection: disconnect_after_n_commands"));
        return false;
    }

    if (fault.enabled && (fault.timeout || FaultInjectionConfig::shouldRandomTimeout(fault))) {
        emit errorOccurred(QStringLiteral("Fault injection: motion timeout"));
        return false;
    }

    xMm_ = xMm;
    yMm_ = yMm;
    zMm_ = zMm;
    emit positionChanged(xMm_, yMm_, zMm_);
    Diagnostics::HardwareSessionRecorder::recordGrbl(QStringLiteral("tx"), QStringLiteral("G1"), QStringLiteral("ok"), true);
    return true;
}

} // namespace NFSScanner::Devices::Motion
