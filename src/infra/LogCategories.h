#pragma once

#include <QString>

namespace NFSScanner::Infra {

enum class LogCategory {
    App,
    Motion,
    Spectrum,
    Camera,
    Probe,
    Scan,
    Analysis,
    Report,
    License,
    HardwareChecklist,
    ScpiRaw,
    GrblRaw
};

QString logCategoryToString(LogCategory category);
QString logsRootDirectory();
QString logFilePath(LogCategory category);
void writeCategoryLog(LogCategory category, const QString &message);
int cleanupOldLogs(int keepDays = 30);

} // namespace NFSScanner::Infra
