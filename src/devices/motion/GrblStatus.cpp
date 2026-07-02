#include "devices/motion/GrblStatus.h"

namespace NFSScanner::Devices::Motion {

QString grblStateToString(GrblState state)
{
    switch (state) {
    case GrblState::Idle:
        return QStringLiteral("Idle");
    case GrblState::Run:
        return QStringLiteral("Run");
    case GrblState::Hold:
        return QStringLiteral("Hold");
    case GrblState::Alarm:
        return QStringLiteral("Alarm");
    case GrblState::Door:
        return QStringLiteral("Door");
    case GrblState::Check:
        return QStringLiteral("Check");
    case GrblState::Home:
        return QStringLiteral("Home");
    case GrblState::Jog:
        return QStringLiteral("Jog");
    case GrblState::Unknown:
        break;
    }
    return QStringLiteral("Unknown");
}

GrblState grblStateFromToken(const QString &token)
{
    const QString t = token.trimmed();
    if (t == QStringLiteral("Idle")) {
        return GrblState::Idle;
    }
    if (t == QStringLiteral("Run")) {
        return GrblState::Run;
    }
    if (t == QStringLiteral("Hold")) {
        return GrblState::Hold;
    }
    if (t.startsWith(QStringLiteral("Alarm"))) {
        return GrblState::Alarm;
    }
    if (t == QStringLiteral("Door")) {
        return GrblState::Door;
    }
    if (t == QStringLiteral("Check")) {
        return GrblState::Check;
    }
    if (t == QStringLiteral("Home")) {
        return GrblState::Home;
    }
    if (t == QStringLiteral("Jog")) {
        return GrblState::Jog;
    }
    return GrblState::Unknown;
}

} // namespace NFSScanner::Devices::Motion
