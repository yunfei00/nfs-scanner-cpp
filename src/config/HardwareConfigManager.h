#pragma once

#include "config/HardwareConfig.h"

#include <QString>
#include <QStringList>

namespace NFSScanner::Config {

class HardwareConfigManager
{
public:
    HardwareConfigManager() = default;

    const HardwareConfig &config() const { return config_; }
    HardwareConfig &config() { return config_; }

    QString configPath() const { return configPath_; }
    void setConfigPath(const QString &path) { configPath_ = path; }

    QString currentProfileName() const { return currentProfileName_; }

    bool load(const QString &path = QString());
    bool save(const QString &path = QString()) const;
    bool ensureDefaultFile(const QString &path = QString());

    QStringList listProfiles() const;
    bool loadProfile(const QString &profileName);
    bool saveProfile(const QString &profileName);
    static bool validateProfile(const HardwareConfig &config,
                                QStringList *errors,
                                QStringList *warnings = nullptr);

    QString lastError() const { return lastError_; }

private:
    void setError(const QString &message) const;
    QString profileFilePath(const QString &profileName) const;

    HardwareConfig config_;
    QString configPath_;
    QString currentProfileName_;
    mutable QString lastError_;
};

} // namespace NFSScanner::Config
