#include "infra/LogCategories.h"

#include "infra/Logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace NFSScanner::Infra {

namespace {

LogCategory mapLegacyCategory(LogCategory category)
{
    return category;
}

} // namespace

QString logCategoryToString(LogCategory category)
{
    switch (category) {
    case LogCategory::App:
        return QStringLiteral("app");
    case LogCategory::Motion:
        return QStringLiteral("motion");
    case LogCategory::Spectrum:
        return QStringLiteral("spectrum");
    case LogCategory::Camera:
        return QStringLiteral("camera");
    case LogCategory::Probe:
        return QStringLiteral("probe");
    case LogCategory::Scan:
        return QStringLiteral("scan");
    case LogCategory::Analysis:
        return QStringLiteral("analysis");
    case LogCategory::Report:
        return QStringLiteral("report");
    case LogCategory::License:
        return QStringLiteral("license");
    case LogCategory::HardwareChecklist:
        return QStringLiteral("hardware_checklist");
    case LogCategory::ScpiRaw:
        return QStringLiteral("scpi");
    case LogCategory::GrblRaw:
        return QStringLiteral("grbl");
    }
    return QStringLiteral("app");
}

QString logsRootDirectory()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString candidate = QDir(appDir).filePath(QStringLiteral("../logs"));
    QDir().mkpath(candidate);
    return QDir(candidate).absolutePath();
}

QString logFilePath(LogCategory category)
{
    const QString date = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd"));
    const QString fileName = QStringLiteral("%1_%2.log").arg(logCategoryToString(mapLegacyCategory(category)), date);
    return QDir(logsRootDirectory()).filePath(fileName);
}

void writeCategoryLog(LogCategory category, const QString &message)
{
    const QString formatted = Logger::format(Logger::Level::Info, message);
    Logger::appendGlobalHistory(formatted);

    QFile file(logFilePath(category));
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        file.write(formatted.toUtf8());
        file.write("\n");
    }
}

int cleanupOldLogs(int keepDays)
{
    if (keepDays <= 0) {
        return 0;
    }

    int removed = 0;
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-keepDays);
    const QFileInfoList files = QDir(logsRootDirectory()).entryInfoList({QStringLiteral("*.log")}, QDir::Files);
    for (const QFileInfo &info : files) {
        if (info.lastModified() < cutoff) {
            if (QFile::remove(info.absoluteFilePath())) {
                ++removed;
            }
        }
    }
    return removed;
}

} // namespace NFSScanner::Infra
