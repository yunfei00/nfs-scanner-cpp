#include "config/HardwareConfigManager.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>

namespace NFSScanner::Config {

void HardwareConfigManager::setError(const QString &message) const
{
    lastError_ = message;
}

bool HardwareConfigManager::ensureDefaultFile(const QString &path)
{
    const QString targetPath = path.isEmpty()
        ? (configPath_.isEmpty() ? defaultHardwareConfigPath() : configPath_)
        : path;
    configPath_ = targetPath;

    if (QFile::exists(targetPath)) {
        return true;
    }

    QDir().mkpath(QFileInfo(targetPath).absolutePath());
    config_ = HardwareConfig::defaults();
    return save(targetPath);
}

bool HardwareConfigManager::load(const QString &path)
{
    lastError_.clear();
    const QString targetPath = path.isEmpty()
        ? (configPath_.isEmpty() ? defaultHardwareConfigPath() : configPath_)
        : path;
    configPath_ = targetPath;

    if (!QFile::exists(targetPath)) {
        config_ = HardwareConfig::defaults();
        if (!save(targetPath)) {
            setError(QStringLiteral("配置文件不存在且无法创建默认配置: %1").arg(targetPath));
            return false;
        }
        return true;
    }

    QFile file(targetPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(QStringLiteral("无法读取硬件配置: %1").arg(targetPath));
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(QStringLiteral("硬件配置 JSON 解析失败 (%1): %2")
                     .arg(targetPath, parseError.errorString()));
        return false;
    }

    HardwareConfig loaded = HardwareConfig::defaults();
    QStringList errors;
    if (!hardwareConfigFromJson(document.object(), &loaded, &errors)) {
        setError(QStringLiteral("硬件配置字段错误:\n%1").arg(errors.join(QStringLiteral("\n"))));
        return false;
    }

    QStringList validateErrors;
    if (!loaded.validate(&validateErrors)) {
        setError(QStringLiteral("硬件配置校验失败:\n%1").arg(validateErrors.join(QStringLiteral("\n"))));
        return false;
    }

    config_ = loaded;
    return true;
}

bool HardwareConfigManager::save(const QString &path) const
{
    const QString targetPath = path.isEmpty()
        ? (configPath_.isEmpty() ? defaultHardwareConfigPath() : configPath_)
        : path;

    QDir().mkpath(QFileInfo(targetPath).absolutePath());
    const QJsonDocument document(hardwareConfigToJson(config_));
    QFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        const_cast<HardwareConfigManager *>(this)->setError(
            QStringLiteral("无法写入硬件配置: %1").arg(targetPath));
        return false;
    }
    file.write(document.toJson(QJsonDocument::Indented));
    return true;
}

QString HardwareConfigManager::profileFilePath(const QString &profileName) const
{
    QString name = profileName.trimmed();
    if (name.isEmpty()) {
        return {};
    }
    if (!name.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
        name.append(QStringLiteral(".json"));
    }
    return QDir(defaultProfilesDirectory()).filePath(name);
}

QStringList HardwareConfigManager::listProfiles() const
{
    QDir dir(defaultProfilesDirectory());
    if (!dir.exists()) {
        return {};
    }

    QStringList names;
    const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString &file : files) {
        names.append(QFileInfo(file).completeBaseName());
    }
    return names;
}

bool HardwareConfigManager::loadProfile(const QString &profileName)
{
    lastError_.clear();
    const QString path = profileFilePath(profileName);
    if (path.isEmpty() || !QFile::exists(path)) {
        setError(QStringLiteral("Profile 不存在: %1").arg(profileName));
        return false;
    }

    if (!load(path)) {
        return false;
    }
    currentProfileName_ = QFileInfo(path).completeBaseName();
    return true;
}

bool HardwareConfigManager::saveProfile(const QString &profileName)
{
    const QString path = profileFilePath(profileName);
    if (path.isEmpty()) {
        setError(QStringLiteral("Profile 名称无效。"));
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    if (!save(path)) {
        return false;
    }
    currentProfileName_ = QFileInfo(path).completeBaseName();
    return true;
}

bool HardwareConfigManager::validateProfile(const HardwareConfig &config,
                                            QStringList *errors,
                                            QStringList *warnings)
{
    QStringList localErrors;
    QStringList localWarnings;
    QStringList *errorTarget = errors ? errors : &localErrors;
    QStringList *warningTarget = warnings ? warnings : &localWarnings;

    if (!config.validate(errorTarget)) {
        return false;
    }

    if (config.camera.type.contains(QStringLiteral("usb"), Qt::CaseInsensitive)
        || config.camera.type.contains(QStringLiteral("industrial"), Qt::CaseInsensitive)) {
        warningTarget->append(QStringLiteral("camera.type 为 Stub，需后续 SDK/OpenCV 接入。"));
    }
    if (config.probe.type.compare(QStringLiteral("serial"), Qt::CaseInsensitive) == 0
        || config.probe.type.compare(QStringLiteral("sdk"), Qt::CaseInsensitive) == 0) {
        warningTarget->append(QStringLiteral("probe.type 为 Stub，需现场确认接线。"));
    }
    if (config.motion.enabled && config.motion.homeOnConnect) {
        warningTarget->append(QStringLiteral("motion.home_on_connect=true，连接后可能自动 Home。"));
    }

    return errorTarget->isEmpty();
}

} // namespace NFSScanner::Config
