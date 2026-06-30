#include "project/Project.h"

#include <QDir>

namespace NFSScanner::Project {

QString ProjectInfo::projectJsonPath() const
{
    return QDir(rootPath).filePath(QStringLiteral("project.json"));
}

QString ProjectInfo::scansDir() const
{
    return QDir(rootPath).filePath(QStringLiteral("scans"));
}

QString ProjectInfo::imagesDir() const
{
    return QDir(rootPath).filePath(QStringLiteral("images"));
}

QString ProjectInfo::reportsDir() const
{
    return QDir(rootPath).filePath(QStringLiteral("reports"));
}

QString ProjectInfo::logsDir() const
{
    return QDir(rootPath).filePath(QStringLiteral("logs"));
}

QString ProjectInfo::exportsDir() const
{
    return QDir(rootPath).filePath(QStringLiteral("exports"));
}

bool ProjectInfo::isOpen() const
{
    return !rootPath.trimmed().isEmpty() && !name.trimmed().isEmpty();
}

} // namespace NFSScanner::Project
