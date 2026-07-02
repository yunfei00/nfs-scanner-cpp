#pragma once

#include <QString>

namespace NFSScanner::Devices::Motion {

struct MotionPosition
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

enum class GrblState {
    Unknown,
    Idle,
    Run,
    Hold,
    Alarm,
    Door,
    Check,
    Home,
    Jog
};

struct GrblStatusReport
{
    GrblState state = GrblState::Unknown;
    QString rawState;
    MotionPosition machinePosition;
    MotionPosition workPosition;
    bool hasMachinePosition = false;
    bool hasWorkPosition = false;
    bool hasWorkCoordinateOffset = false;
    double wcoX = 0.0;
    double wcoY = 0.0;
    double wcoZ = 0.0;
    QString feedRate;
    QString rawLine;
};

QString grblStateToString(GrblState state);
GrblState grblStateFromToken(const QString &token);

} // namespace NFSScanner::Devices::Motion
