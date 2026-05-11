#include "infra/Logger.h"

#include <QDateTime>

namespace NFSScanner::Infra {

Logger::Logger(QObject *parent)
    : QObject(parent)
{
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
    emit messageReady(format(level, message));
}

} // namespace NFSScanner::Infra
