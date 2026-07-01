#include "project/ProjectManager.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>

#include <functional>

namespace NFSScanner::Project {

ProjectManager::ProjectManager(QObject *parent)
    : QObject(parent)
    , workspaceRoot_(QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                         .filePath(QStringLiteral("workspace")))
{
    ensureWorkspaceStructure();
}

bool ProjectManager::hasOpenProject() const
{
    return project_.isOpen();
}

ProjectInfo ProjectManager::currentProject() const
{
    return project_;
}

QString ProjectManager::workspaceRoot() const
{
    return workspaceRoot_;
}

QString ProjectManager::defaultScanOutputDir() const
{
    if (project_.isOpen()) {
        return project_.scansDir();
    }
    return QDir(workspaceRoot_).filePath(QStringLiteral("scans"));
}

QString ProjectManager::defaultReportsDir() const
{
    if (project_.isOpen()) {
        return project_.reportsDir();
    }
    return QDir(workspaceRoot_).filePath(QStringLiteral("reports"));
}

QStringList ProjectManager::listScanTaskDirs() const
{
    QStringList tasks;
    const QDir scansDir(defaultScanOutputDir());
    if (!scansDir.exists()) {
        return tasks;
    }

    const QFileInfoList entries = scansDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        const QString taskDir = entry.absoluteFilePath();
        const bool hasTraces = QFile::exists(QDir(taskDir).filePath(QStringLiteral("traces.csv")));
        const bool hasMeta = QFile::exists(QDir(taskDir).filePath(QStringLiteral("meta.json")));
        if (hasTraces || hasMeta) {
            tasks.prepend(taskDir);
        }
    }
    return tasks;
}

void ProjectManager::ensureWorkspaceStructure() const
{
    QDir().mkpath(workspaceRoot_);
    QDir().mkpath(defaultScanOutputDir());
    QDir().mkpath(defaultReportsDir());
}

bool ProjectManager::createProject(const QString &name, const QString &parentDirectory)
{
    lastError_.clear();
    const QString safeName = name.trimmed();
    if (safeName.isEmpty()) {
        setLastError(QStringLiteral("项目名称不能为空。"));
        return false;
    }

    const QString rootPath = QDir(parentDirectory.trimmed().isEmpty() ? workspaceRoot_ : parentDirectory)
                                 .filePath(safeName);
    if (QDir(rootPath).exists()) {
        setLastError(QStringLiteral("项目目录已存在：%1").arg(rootPath));
        return false;
    }

    if (!ensureProjectStructure(rootPath)) {
        return false;
    }

    project_.name = safeName;
    project_.rootPath = QDir(rootPath).absolutePath();
    project_.createdAt = QDateTime::currentDateTime();
    project_.modifiedAt = project_.createdAt;

    if (!writeProjectJson()) {
        return false;
    }

    addRecentProject(project_.rootPath);
    emit projectChanged(project_);
    emit logMessage(QStringLiteral("已创建项目：%1").arg(project_.rootPath));
    return true;
}

bool ProjectManager::openProject(const QString &projectRootPath)
{
    lastError_.clear();
    const QDir dir(projectRootPath);
    if (!dir.exists()) {
        setLastError(QStringLiteral("项目目录不存在：%1").arg(projectRootPath));
        return false;
    }

    if (!readProjectJson(dir.absolutePath())) {
        return false;
    }

    addRecentProject(project_.rootPath);
    emit projectChanged(project_);
    emit logMessage(QStringLiteral("已打开项目：%1").arg(project_.rootPath));
    return true;
}

bool ProjectManager::saveProject()
{
    if (!project_.isOpen()) {
        setLastError(QStringLiteral("当前没有打开的项目。"));
        return false;
    }
    project_.modifiedAt = QDateTime::currentDateTime();
    if (!writeProjectJson()) {
        return false;
    }
    emit logMessage(QStringLiteral("项目已保存。"));
    return true;
}

bool ProjectManager::saveProjectAs(const QString &newRootPath)
{
    lastError_.clear();
    if (!project_.isOpen()) {
        setLastError(QStringLiteral("当前没有打开的项目。"));
        return false;
    }

    const QString targetRoot = QDir(newRootPath).absolutePath();
    if (QDir(targetRoot).exists()) {
        setLastError(QStringLiteral("目标目录已存在：%1").arg(targetRoot));
        return false;
    }

    std::function<bool(const QString &, const QString &)> copyRecursive;
    copyRecursive = [&](const QString &srcPath, const QString &dstPath) -> bool {
        QDir srcDir(srcPath);
        if (!QDir().mkpath(dstPath)) {
            return false;
        }
        const QFileInfoList entries = srcDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
        for (const QFileInfo &entry : entries) {
            const QString src = entry.absoluteFilePath();
            const QString dst = QDir(dstPath).filePath(entry.fileName());
            if (entry.isDir()) {
                if (!copyRecursive(src, dst)) {
                    return false;
                }
            } else if (!QFile::copy(src, dst)) {
                return false;
            }
        }
        return true;
    };

    if (!copyRecursive(project_.rootPath, targetRoot)) {
        setLastError(QStringLiteral("复制项目文件失败。"));
        return false;
    }

    project_.rootPath = targetRoot;
    project_.modifiedAt = QDateTime::currentDateTime();
    return writeProjectJson();
}

void ProjectManager::closeProject()
{
    project_ = ProjectInfo{};
    emit projectChanged(project_);
    emit logMessage(QStringLiteral("项目已关闭，使用默认 workspace。"));
}

QStringList ProjectManager::recentProjects() const
{
    QSettings settings;
    return settings.value(QStringLiteral("recentProjects")).toStringList();
}

QString ProjectManager::lastError() const
{
    return lastError_;
}

bool ProjectManager::ensureProjectStructure(const QString &rootPath)
{
    QDir dir(rootPath);
    const QStringList subdirs{
        QStringLiteral("scans"),
        QStringLiteral("images"),
        QStringLiteral("reports"),
        QStringLiteral("logs"),
        QStringLiteral("exports"),
    };
    if (!dir.mkpath(QStringLiteral("."))) {
        setLastError(QStringLiteral("无法创建项目根目录。"));
        return false;
    }
    for (const QString &sub : subdirs) {
        if (!dir.mkpath(sub)) {
            setLastError(QStringLiteral("无法创建子目录：%1").arg(sub));
            return false;
        }
    }
    return true;
}

bool ProjectManager::writeProjectJson()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("name"), project_.name);
    obj.insert(QStringLiteral("root_path"), project_.rootPath);
    obj.insert(QStringLiteral("operator"), project_.operatorName);
    obj.insert(QStringLiteral("created_at"), project_.createdAt.toString(Qt::ISODateWithMs));
    obj.insert(QStringLiteral("modified_at"), project_.modifiedAt.toString(Qt::ISODateWithMs));
    obj.insert(QStringLiteral("notes"), project_.notes);

    QFile file(project_.projectJsonPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setLastError(QStringLiteral("无法写入 project.json。"));
        return false;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return true;
}

bool ProjectManager::readProjectJson(const QString &rootPath)
{
    QFile file(QDir(rootPath).filePath(QStringLiteral("project.json")));
    if (!file.open(QIODevice::ReadOnly)) {
        setLastError(QStringLiteral("无法读取 project.json。"));
        return false;
    }

    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    project_.rootPath = rootPath;
    project_.name = obj.value(QStringLiteral("name")).toString();
    project_.operatorName = obj.value(QStringLiteral("operator")).toString();
    project_.createdAt = QDateTime::fromString(obj.value(QStringLiteral("created_at")).toString(), Qt::ISODateWithMs);
    project_.modifiedAt = QDateTime::fromString(obj.value(QStringLiteral("modified_at")).toString(), Qt::ISODateWithMs);
    project_.notes = obj.value(QStringLiteral("notes")).toString();
    if (project_.name.isEmpty()) {
        project_.name = QDir(rootPath).dirName();
    }
    return true;
}

void ProjectManager::addRecentProject(const QString &path)
{
    QStringList recent = recentProjects();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 8) {
        recent.removeLast();
    }
    QSettings settings;
    settings.setValue(QStringLiteral("recentProjects"), recent);
}

void ProjectManager::setLastError(const QString &message)
{
    lastError_ = message;
}

} // namespace NFSScanner::Project
