#include "diagnostics/HardwareSessionRecorder.h"

#include "infra/LogCategories.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <QVector>

namespace NFSScanner::Diagnostics {

QString HardwareSessionRecorder::sessionPath_;

QString HardwareSessionRecorder::summarizeTrace(const QVector<double> &values, int maxPoints)
{
    if (values.isEmpty()) {
        return QStringLiteral("empty");
    }
    QStringList parts;
    const int count = qMin(maxPoints, static_cast<int>(values.size()));
    parts << QStringLiteral("points=%1").arg(values.size());
    for (int i = 0; i < count; ++i) {
        parts << QString::number(values.at(i), 'f', 4);
    }
    return parts.join(QStringLiteral("; "));
}

bool HardwareSessionRecorder::startSession(const QString &sessionId)
{
    const QString stamp = sessionId.isEmpty()
        ? QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))
        : sessionId;
    const QString dir = QDir(Infra::logsRootDirectory()).filePath(QStringLiteral("hardware_sessions"));
    QDir().mkpath(dir);
    sessionPath_ = QDir(dir).filePath(QStringLiteral("session_%1.jsonl").arg(stamp));
    recordEvent(QStringLiteral("scan"), QStringLiteral("Session"), QStringLiteral("session started"));
    return true;
}

QString HardwareSessionRecorder::currentSessionPath()
{
    return sessionPath_;
}

void HardwareSessionRecorder::record(const Entry &entry)
{
    if (sessionPath_.isEmpty()) {
        startSession();
    }

    QJsonObject object;
    object.insert(QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("category"), entry.category);
    object.insert(QStringLiteral("direction"), entry.direction);
    object.insert(QStringLiteral("device"), entry.device);
    object.insert(QStringLiteral("command"), entry.command);
    object.insert(QStringLiteral("response_summary"), entry.responseSummary);
    object.insert(QStringLiteral("elapsed_ms"), entry.elapsedMs);
    object.insert(QStringLiteral("success"), entry.success);
    object.insert(QStringLiteral("error"), entry.error);

    QFile file(sessionPath_);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
        file.write("\n");
    }
}

void HardwareSessionRecorder::recordGrbl(const QString &direction,
                                         const QString &command,
                                         const QString &response,
                                         bool success,
                                         const QString &error)
{
    Entry entry;
    entry.category = QStringLiteral("grbl");
    entry.direction = direction;
    entry.device = QStringLiteral("GRBL");
    entry.command = command;
    entry.responseSummary = response.left(120);
    entry.success = success;
    entry.error = error;
    record(entry);
}

void HardwareSessionRecorder::recordScpi(const QString &device,
                                         const QString &direction,
                                         const QString &command,
                                         const QString &response,
                                         qint64 elapsedMs,
                                         bool success,
                                         const QString &error)
{
    Entry entry;
    entry.category = QStringLiteral("scpi");
    entry.direction = direction;
    entry.device = device;
    entry.command = command;
    entry.responseSummary = response.left(120);
    entry.elapsedMs = elapsedMs;
    entry.success = success;
    entry.error = error;
    record(entry);
}

void HardwareSessionRecorder::recordEvent(const QString &category,
                                          const QString &device,
                                          const QString &message,
                                          bool success)
{
    Entry entry;
    entry.category = category;
    entry.direction = QStringLiteral("event");
    entry.device = device;
    entry.command = message;
    entry.responseSummary = message;
    entry.success = success;
    record(entry);
}

bool HardwareSessionRecorder::exportSession(const QString &targetPath)
{
    if (sessionPath_.isEmpty() || !QFile::exists(sessionPath_)) {
        return false;
    }
    return QFile::copy(sessionPath_, targetPath);
}

QStringList HardwareSessionRecorder::listSessions()
{
    const QDir dir(QDir(Infra::logsRootDirectory()).filePath(QStringLiteral("hardware_sessions")));
    if (!dir.exists()) {
        return {};
    }
    QStringList files;
    for (const QFileInfo &info : dir.entryInfoList({QStringLiteral("session_*.jsonl")}, QDir::Files, QDir::Time)) {
        files.append(info.absoluteFilePath());
    }
    return files;
}

} // namespace NFSScanner::Diagnostics
