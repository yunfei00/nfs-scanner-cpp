#pragma once

#include "devices/motion/GrblStatus.h"

#include <optional>
#include <QString>

namespace NFSScanner::Devices::Motion {

class GrblCommandBuilder
{
public:
    static QString buildQueryStatus();
    static QString buildHome();
    static QString buildUnlock();
    static QString buildVersionQuery();
    static QString buildFeedHold();
    static QString buildCycleStart();
    static QByteArray buildSoftReset();

    static QString buildAbsoluteMove(const MotionPosition &target, double feed);
    static QString buildJogDelta(const QString &axis, double delta, double feed);
};

} // namespace NFSScanner::Devices::Motion
