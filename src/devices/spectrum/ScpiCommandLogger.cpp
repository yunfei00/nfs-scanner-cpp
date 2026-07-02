#include "devices/spectrum/ScpiCommandLogger.h"

#include "infra/LogCategories.h"

#include <QDateTime>

namespace NFSScanner::Devices::Spectrum {

namespace {

QString gLastError;
QString gLastIdn;

QString summarizeResponse(const QString &response)
{
    const QString trimmed = response.trimmed();
    if (trimmed.size() <= 120) {
        return trimmed;
    }
    return trimmed.left(120) + QStringLiteral("...");
}

} // namespace

void ScpiCommandLogger::logEntry(const Entry &entry)
{
    const QString line = QStringLiteral("[%1] %2:%3 %4 -> %5 (%6 ms) success=%7 err=%8")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                  entry.deviceType,
                                  entry.host,
                                  entry.command,
                                  entry.responseSummary,
                                  QString::number(entry.elapsedMs),
                                  entry.success ? QStringLiteral("yes") : QStringLiteral("no"),
                                  entry.errorMessage);

    Infra::writeCategoryLog(Infra::LogCategory::ScpiRaw, line);

    if (!entry.success && !entry.errorMessage.isEmpty()) {
        gLastError = entry.errorMessage;
    }
    if (entry.command.contains(QStringLiteral("*IDN"), Qt::CaseInsensitive) && entry.success) {
        gLastIdn = entry.responseSummary;
    }
}

QString ScpiCommandLogger::lastErrorMessage()
{
    return gLastError;
}

QString ScpiCommandLogger::lastIdnResponse()
{
    return gLastIdn;
}

void ScpiCommandLogger::clearRecentErrors()
{
    gLastError.clear();
}

} // namespace NFSScanner::Devices::Spectrum
