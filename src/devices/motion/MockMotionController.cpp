#include "devices/motion/MockMotionController.h"

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
    if (connected_) {
        return true;
    }

    connected_ = true;
    emit connectionChanged(true);
    emit positionChanged(xMm_, yMm_, zMm_);
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
    if (!connected_) {
        emit errorOccurred(QStringLiteral("运动控制器未连接，无法移动。"));
        return false;
    }

    xMm_ = xMm;
    yMm_ = yMm;
    zMm_ = zMm;
    emit positionChanged(xMm_, yMm_, zMm_);
    return true;
}

} // namespace NFSScanner::Devices::Motion
