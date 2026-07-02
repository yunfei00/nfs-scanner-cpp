#include "devices/motion/GrblResponseParser.h"

#include <QStringList>

namespace NFSScanner::Devices::Motion {

bool GrblResponseParser::parsePositionField(const QString &field, MotionPosition *position)
{
    if (!position) {
        return false;
    }

    const int colon = field.indexOf(QLatin1Char(':'));
    if (colon < 0) {
        return false;
    }

    const QStringList values = field.mid(colon + 1).split(QLatin1Char(','));
    if (values.size() != 3) {
        return false;
    }

    bool xOk = false;
    bool yOk = false;
    bool zOk = false;
    const double x = values.at(0).toDouble(&xOk);
    const double y = values.at(1).toDouble(&yOk);
    const double z = values.at(2).toDouble(&zOk);
    if (!xOk || !yOk || !zOk) {
        return false;
    }

    position->x = x;
    position->y = y;
    position->z = z;
    return true;
}

GrblParseResult GrblResponseParser::parseLine(const QString &line)
{
    GrblParseResult result;
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
        return result;
    }

    const QString lower = trimmed.toLower();
    if (lower == QStringLiteral("ok")) {
        result.ok = true;
        result.isOkResponse = true;
        return result;
    }

    if (lower.startsWith(QStringLiteral("error:"))) {
        result.ok = true;
        result.isErrorResponse = true;
        result.errorMessage = trimmed;
        const QString codeText = trimmed.mid(6).trimmed();
        bool codeOk = false;
        result.errorCode = codeText.toInt(&codeOk);
        if (!codeOk) {
            result.errorCode = -1;
        }
        return result;
    }

    if (!trimmed.startsWith(QLatin1Char('<')) || !trimmed.endsWith(QLatin1Char('>'))) {
        result.ok = true;
        return result;
    }

    result.ok = true;
    result.isStatusReport = true;
    result.status.rawLine = trimmed;

    const QString payload = trimmed.mid(1, trimmed.size() - 2);
    const QStringList fields = payload.split(QLatin1Char('|'), Qt::SkipEmptyParts);
    if (fields.isEmpty()) {
        return result;
    }

    result.status.rawState = fields.first();
    result.status.state = grblStateFromToken(result.status.rawState);

    for (const QString &field : fields) {
        if (field.startsWith(QStringLiteral("MPos:"))) {
            MotionPosition pos;
            if (parsePositionField(field, &pos)) {
                result.status.machinePosition = pos;
                result.status.hasMachinePosition = true;
            }
        } else if (field.startsWith(QStringLiteral("WPos:"))) {
            MotionPosition pos;
            if (parsePositionField(field, &pos)) {
                result.status.workPosition = pos;
                result.status.hasWorkPosition = true;
            }
        } else if (field.startsWith(QStringLiteral("WCO:"))) {
            MotionPosition wco;
            if (parsePositionField(field, &wco)) {
                result.status.wcoX = wco.x;
                result.status.wcoY = wco.y;
                result.status.wcoZ = wco.z;
                result.status.hasWorkCoordinateOffset = true;
            }
        } else if (field.startsWith(QStringLiteral("FS:"))) {
            result.status.feedRate = field.mid(3);
        }
    }

    return result;
}

bool GrblResponseParser::isIdleState(GrblState state)
{
    return state == GrblState::Idle;
}

bool GrblResponseParser::isAlarmState(GrblState state)
{
    return state == GrblState::Alarm;
}

} // namespace NFSScanner::Devices::Motion
