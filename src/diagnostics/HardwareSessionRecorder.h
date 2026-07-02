#pragma once

#include <QString>
#include <QStringList>

#include <QVector>

namespace NFSScanner::Diagnostics {

class HardwareSessionRecorder
{
public:
    struct Entry
    {
        QString category;
        QString direction;
        QString device;
        QString command;
        QString responseSummary;
        qint64 elapsedMs = 0;
        bool success = true;
        QString error;
    };

    static bool startSession(const QString &sessionId = QString());
    static QString currentSessionPath();
    static void record(const Entry &entry);
    static void recordGrbl(const QString &direction, const QString &command, const QString &response, bool success, const QString &error = QString());
    static void recordScpi(const QString &device, const QString &direction, const QString &command, const QString &response, qint64 elapsedMs, bool success, const QString &error = QString());
    static void recordEvent(const QString &category, const QString &device, const QString &message, bool success = true);
    static bool exportSession(const QString &targetPath);
    static QStringList listSessions();
    static QString summarizeTrace(const QVector<double> &values, int maxPoints = 5);

private:
    static QString sessionPath_;
};

} // namespace NFSScanner::Diagnostics
