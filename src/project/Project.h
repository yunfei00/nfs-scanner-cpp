#pragma once

#include <QDateTime>
#include <QString>

namespace NFSScanner::Project {

struct ProjectInfo
{
    QString name;
    QString rootPath;
    QString operatorName;
    QDateTime createdAt;
    QDateTime modifiedAt;
    QString notes;

    QString projectJsonPath() const;
    QString scansDir() const;
    QString imagesDir() const;
    QString reportsDir() const;
    QString logsDir() const;
    QString exportsDir() const;

    bool isOpen() const;
};

} // namespace NFSScanner::Project
