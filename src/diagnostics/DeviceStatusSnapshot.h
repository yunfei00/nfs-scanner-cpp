#pragma once

#include <QJsonObject>
#include <QString>

namespace NFSScanner::Core {
class DeviceManager;
}

namespace NFSScanner::License {
class LicenseManager;
}

namespace NFSScanner::Diagnostics {

class DeviceStatusSnapshot
{
public:
    static bool capture(Core::DeviceManager *deviceManager,
                        License::LicenseManager *licenseManager,
                        const QString &profileName,
                        QJsonObject *object);
    static bool saveToFile(const QString &path,
                           Core::DeviceManager *deviceManager,
                           License::LicenseManager *licenseManager,
                           const QString &profileName);
    static QString summaryText(const QJsonObject &object);
};

} // namespace NFSScanner::Diagnostics
