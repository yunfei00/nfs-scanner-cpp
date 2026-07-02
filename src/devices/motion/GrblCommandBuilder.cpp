#include "devices/motion/GrblCommandBuilder.h"

#include <cmath>

namespace NFSScanner::Devices::Motion {

namespace {

QString mmText(double value)
{
    return QString::number(value, 'f', 3);
}

QString feedText(double feed)
{
    const double rounded = std::round(feed);
    if (std::abs(feed - rounded) < 0.000001) {
        return QString::number(static_cast<int>(rounded));
    }
    return QString::number(feed, 'f', 3);
}

} // namespace

QString GrblCommandBuilder::buildQueryStatus()
{
    return QStringLiteral("?");
}

QString GrblCommandBuilder::buildHome()
{
    return QStringLiteral("$H");
}

QString GrblCommandBuilder::buildUnlock()
{
    return QStringLiteral("$X");
}

QString GrblCommandBuilder::buildVersionQuery()
{
    return QStringLiteral("$I");
}

QString GrblCommandBuilder::buildFeedHold()
{
    return QStringLiteral("!");
}

QString GrblCommandBuilder::buildCycleStart()
{
    return QStringLiteral("~");
}

QByteArray GrblCommandBuilder::buildSoftReset()
{
    return QByteArray(1, static_cast<char>(0x18));
}

QString GrblCommandBuilder::buildAbsoluteMove(const MotionPosition &target, double feed)
{
    return QStringLiteral("G1X%1Y%2Z%3F%4")
        .arg(mmText(target.x), mmText(target.y), mmText(target.z), feedText(feed));
}

QString GrblCommandBuilder::buildJogDelta(const QString &axis, double delta, double feed)
{
    const QString normalized = axis.trimmed().toUpper();
    if (normalized == QStringLiteral("X")) {
        return QStringLiteral("G91G1X%1F%2").arg(mmText(delta), feedText(feed));
    }
    if (normalized == QStringLiteral("Y")) {
        return QStringLiteral("G91G1Y%1F%2").arg(mmText(delta), feedText(feed));
    }
    if (normalized == QStringLiteral("Z")) {
        return QStringLiteral("G91G1Z%1F%2").arg(mmText(delta), feedText(feed));
    }
    return {};
}

} // namespace NFSScanner::Devices::Motion
