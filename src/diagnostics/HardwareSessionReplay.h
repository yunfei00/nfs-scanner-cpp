#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace NFSScanner::Diagnostics {

struct SessionReplayEntry
{
    QString timestamp;
    QString category;
    QString direction;
    QString device;
    QString command;
    QString responseSummary;
    qint64 elapsedMs = 0;
    bool success = true;
    QString error;
};

class HardwareSessionReplay
{
public:
    static bool loadSession(const QString &path, QVector<SessionReplayEntry> *entries, QString *error = nullptr);
    static bool replayGrblStatus(const QVector<SessionReplayEntry> &entries, QString *idleLine = nullptr, QString *alarmLine = nullptr);
    static bool replayScpiIdn(const QVector<SessionReplayEntry> &entries, QString *idn = nullptr);
    static bool replayTraceSummary(const QVector<SessionReplayEntry> &entries, QString *summary = nullptr);
    static int countByCategory(const QVector<SessionReplayEntry> &entries, const QString &category);
};

} // namespace NFSScanner::Diagnostics
