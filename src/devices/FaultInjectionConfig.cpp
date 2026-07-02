#include "devices/FaultInjectionConfig.h"

#include "config/HardwareConfig.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

namespace NFSScanner::Devices {

namespace {

FaultInjectionConfig gFaultConfig;

bool readBool(const QJsonObject &object, const char *key)
{
    return object.value(QString::fromLatin1(key)).toBool(false);
}

double readDouble(const QJsonObject &object, const char *key)
{
    return object.value(QString::fromLatin1(key)).toDouble(0.0);
}

int readInt(const QJsonObject &object, const char *key)
{
    return object.value(QString::fromLatin1(key)).toInt(0);
}

} // namespace

FaultInjectionConfig FaultInjectionConfig::disabled()
{
    return FaultInjectionConfig{};
}

bool FaultInjectionConfig::loadFromProfile(const QString &profileName,
                                         FaultInjectionConfig *config,
                                         QString *error)
{
    if (!config) {
        return false;
    }
    *config = disabled();

    const QString path = QDir(Config::defaultProfilesDirectory()).filePath(profileName + QStringLiteral(".json"));
    if (!QFile::exists(path)) {
        if (error) {
            *error = QStringLiteral("Profile not found: %1").arg(path);
        }
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot read profile: %1").arg(path);
        }
        return false;
    }

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonObject fault = root.value(QStringLiteral("fault_injection")).toObject();
    if (fault.isEmpty()) {
        return true;
    }

    config->enabled = true;
    config->connectFail = readBool(fault, "connect_fail");
    config->timeout = readBool(fault, "timeout");
    config->randomTimeoutRate = readDouble(fault, "random_timeout_rate");
    config->invalidResponse = readBool(fault, "invalid_response");
    config->disconnectAfterNCommands = readInt(fault, "disconnect_after_n_commands");
    config->motionAlarm = readBool(fault, "motion_alarm");
    config->limitError = readBool(fault, "limit_error");
    config->spectrumEmptyTrace = readBool(fault, "spectrum_empty_trace");
    config->spectrumBadCsv = readBool(fault, "spectrum_bad_csv");
    config->cameraCaptureFail = readBool(fault, "camera_capture_fail");
    config->probeSwitchFail = readBool(fault, "probe_switch_fail");
    config->resetCounters();
    return true;
}

bool FaultInjectionConfig::shouldRandomTimeout(const FaultInjectionConfig &config)
{
    if (!config.enabled || config.randomTimeoutRate <= 0.0) {
        return false;
    }
    return QRandomGenerator::global()->generateDouble() < config.randomTimeoutRate;
}

void FaultInjectionConfig::onCommandSent()
{
    ++commandCount_;
}

void FaultInjectionConfig::resetCounters()
{
    commandCount_ = 0;
}

FaultInjectionConfig &globalFaultInjectionConfig()
{
    return gFaultConfig;
}

} // namespace NFSScanner::Devices
