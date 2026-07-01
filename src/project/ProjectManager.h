#pragma once

#include "project/Project.h"

#include <QObject>
#include <QStringList>

namespace NFSScanner::Project {

class ProjectManager final : public QObject
{
    Q_OBJECT

public:
    explicit ProjectManager(QObject *parent = nullptr);

    bool hasOpenProject() const;
    ProjectInfo currentProject() const;
    QString workspaceRoot() const;
    QString defaultScanOutputDir() const;
    QString defaultReportsDir() const;
    QStringList listScanTaskDirs() const;

    bool createProject(const QString &name, const QString &parentDirectory);
    bool openProject(const QString &projectRootPath);
    bool saveProject();
    bool saveProjectAs(const QString &newRootPath);
    void closeProject();

    QStringList recentProjects() const;
    QString lastError() const;

signals:
    void projectChanged(const ProjectInfo &info);
    void logMessage(const QString &message);

private:
    bool ensureProjectStructure(const QString &rootPath);
    void ensureWorkspaceStructure() const;
    bool writeProjectJson();
    bool readProjectJson(const QString &rootPath);
    void addRecentProject(const QString &path);
    void setLastError(const QString &message);

    ProjectInfo project_;
    QString workspaceRoot_;
    QString lastError_;
};

} // namespace NFSScanner::Project
