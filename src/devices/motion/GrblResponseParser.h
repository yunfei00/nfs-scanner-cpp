#pragma once

#include "devices/motion/GrblStatus.h"

#include <QString>

namespace NFSScanner::Devices::Motion {

struct GrblParseResult
{
    bool ok = false;
    bool isStatusReport = false;
    bool isOkResponse = false;
    bool isErrorResponse = false;
    int errorCode = -1;
    QString errorMessage;
    GrblStatusReport status;
};

class GrblResponseParser
{
public:
    static GrblParseResult parseLine(const QString &line);
    static bool parsePositionField(const QString &field, MotionPosition *position);
    static bool isIdleState(GrblState state);
    static bool isAlarmState(GrblState state);
};

} // namespace NFSScanner::Devices::Motion
