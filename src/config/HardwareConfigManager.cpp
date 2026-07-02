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

} // namespace NFSScanner::Config
