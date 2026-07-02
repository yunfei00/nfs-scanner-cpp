#pragma once

#include "config/HardwareConfig.h"

#include <QString>

namespace NFSScanner::Config {

class HardwareConfigManager
{
public:
    HardwareConfigManager() = default;

    const HardwareConfig &config() const { return config_; }
    HardwareConfig &config() { return config_; }

    QString configPath() const { return configPath_; }
    void setConfigPath(const QString &path) { configPath_ = path; }

    bool load(const QString &path = QString());
    bool save(const QString &path = QString()) const;
    bool ensureDefaultFile(const QString &path = QString());

    QString lastError() const { return lastError_; }

private:
    void setError(const QString &message) const;

    HardwareConfig config_;
    QString configPath_;
    mutable QString lastError_;
};

} // namespace NFSScanner::Config
