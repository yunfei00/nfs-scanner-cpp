#pragma once

#include "config/HardwareConfig.h"

#include <QString>

namespace NFSScanner::Core {
class DeviceManager;
}

namespace NFSScanner::Diagnostics {

class HardwareSnapshotWriter
{
public:
    static bool writeHardwareConfigSnapshot(const QString &directory,
                                            const Config::HardwareConfig &config,
                                            const QString &profileName);
    static bool writeDeviceStatusSnapshot(const QString &directory, const Core::DeviceManager *deviceManager);
};

} // namespace NFSScanner::Diagnostics
