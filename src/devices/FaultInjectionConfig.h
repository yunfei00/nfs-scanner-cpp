#pragma once

#include <QString>

namespace NFSScanner::Devices {

struct FaultInjectionConfig
{
    bool enabled = false;
    bool connectFail = false;
    bool timeout = false;
    double randomTimeoutRate = 0.0;
    bool invalidResponse = false;
    int disconnectAfterNCommands = 0;
    bool motionAlarm = false;
    bool limitError = false;
    bool spectrumEmptyTrace = false;
    bool spectrumBadCsv = false;
    bool cameraCaptureFail = false;
    bool probeSwitchFail = false;

    static FaultInjectionConfig disabled();
    static bool loadFromProfile(const QString &profileName, FaultInjectionConfig *config, QString *error = nullptr);
    static bool shouldRandomTimeout(const FaultInjectionConfig &config);
    void onCommandSent();
    void resetCounters();

    int commandCount() const { return commandCount_; }

private:
    int commandCount_ = 0;
};

FaultInjectionConfig &globalFaultInjectionConfig();

} // namespace NFSScanner::Devices
