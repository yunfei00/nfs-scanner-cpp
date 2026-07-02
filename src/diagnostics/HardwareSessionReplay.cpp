#include "diagnostics/HardwareSessionReplay.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace NFSScanner::Diagnostics {

bool HardwareSessionReplay::loadSession(const QString &path,
                                        QVector<SessionReplayEntry> *entries,
                                        QString *error)
{
    if (!entries) {
        return false;
    }
    entries->clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("Cannot open session: %1").arg(path);
        }
        return false;
    }

    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QJsonObject object = QJsonDocument::fromJson(line).object();
        if (object.isEmpty()) {
            continue;
        }
        SessionReplayEntry entry;
        entry.timestamp = object.value(QStringLiteral("timestamp")).toString();
        entry.category = object.value(QStringLiteral("category")).toString();
        entry.direction = object.value(QStringLiteral("direction")).toString();
        entry.device = object.value(QStringLiteral("device")).toString();
        entry.command = object.value(QStringLiteral("command")).toString();
        entry.responseSummary = object.value(QStringLiteral("response_summary")).toString();
        entry.elapsedMs = object.value(QStringLiteral("elapsed_ms")).toInteger();
        entry.success = object.value(QStringLiteral("success")).toBool(true);
        entry.error = object.value(QStringLiteral("error")).toString();
        entries->append(entry);
    }
    return !entries->isEmpty();
}

bool HardwareSessionReplay::replayGrblStatus(const QVector<SessionReplayEntry> &entries,
                                             QString *idleLine,
                                             QString *alarmLine)
{
    for (const SessionReplayEntry &entry : entries) {
        if (entry.category != QStringLiteral("grbl")) {
            continue;
        }
        if (entry.responseSummary.contains(QStringLiteral("Idle"), Qt::CaseInsensitive) && idleLine) {
            *idleLine = entry.responseSummary;
        }
        if (entry.responseSummary.contains(QStringLiteral("Alarm"), Qt::CaseInsensitive) && alarmLine) {
            *alarmLine = entry.responseSummary;
        }
    }
    return idleLine && !idleLine->isEmpty();
}

bool HardwareSessionReplay::replayScpiIdn(const QVector<SessionReplayEntry> &entries, QString *idn)
{
    for (const SessionReplayEntry &entry : entries) {
        if (entry.category != QStringLiteral("scpi")) {
            continue;
        }
        if (entry.command.contains(QStringLiteral("*IDN"), Qt::CaseInsensitive) && entry.success) {
            if (idn) {
                *idn = entry.responseSummary;
            }
            return true;
        }
    }
    return false;
}

bool HardwareSessionReplay::replayTraceSummary(const QVector<SessionReplayEntry> &entries, QString *summary)
{
    for (const SessionReplayEntry &entry : entries) {
        if (entry.category == QStringLiteral("scpi") && entry.responseSummary.contains(QStringLiteral("points="))) {
            if (summary) {
                *summary = entry.responseSummary;
            }
            return true;
        }
    }
    return false;
}

int HardwareSessionReplay::countByCategory(const QVector<SessionReplayEntry> &entries, const QString &category)
{
    int count = 0;
    for (const SessionReplayEntry &entry : entries) {
        if (entry.category == category) {
            ++count;
        }
    }
    return count;
}

} // namespace NFSScanner::Diagnostics
