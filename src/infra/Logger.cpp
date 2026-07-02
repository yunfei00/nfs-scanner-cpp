#include "infra/Logger.h"

#include <QDateTime>

#include <algorithm>

namespace NFSScanner::Infra {

namespace {

QStringList &globalLogHistory()
{
    static QStringList history;
    return history;
}

constexpr int kMaxHistoryLines = 500;

} // namespace

Logger::Logger(QObject *parent)
    : QObject(parent)
{
}

QStringList Logger::recentLines(int maxCount)
{
    const QStringList &history = globalLogHistory();
    if (maxCount <= 0 || history.isEmpty()) {
        return {};
    }
    const int start = std::max(0, static_cast<int>(history.size()) - maxCount);
    return history.mid(start);
}

void Logger::appendGlobalHistory(const QString &line)
{
    QStringList &history = globalLogHistory();
    history.append(line);
    while (history.size() > kMaxHistoryLines) {
        history.removeFirst();
    }
}

QString Logger::format(Level level, const QString &message)
{
    QString levelText;
    switch (level) {
    case Level::Info:
        levelText = QStringLiteral("INFO");
        break;
    case Level::Warning:
        levelText = QStringLiteral("WARN");
        break;
    case Level::Error:
        levelText = QStringLiteral("ERROR");
        break;
    }

    return QStringLiteral("[%1] [%2] %3")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")),
             levelText,
             message);
}

void Logger::info(const QString &message)
{
    emitMessage(Level::Info, message);
}

void Logger::warning(const QString &message)
{
    emitMessage(Level::Warning, message);
}

void Logger::error(const QString &message)
{
    emitMessage(Level::Error, message);
}

void Logger::emitMessage(Level level, const QString &message)
{
    const QString formatted = format(level, message);
    appendGlobalHistory(formatted);
    emit messageReady(formatted);
}

} // namespace NFSScanner::Infra
