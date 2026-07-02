#pragma once

#include <QString>

namespace NFSScanner::Core {
class DeviceManager;
}

namespace NFSScanner::License {
class LicenseManager;
}

namespace NFSScanner::Project {
class ProjectManager;
}

namespace NFSScanner::Diagnostics {

struct DiagnosticPackageOptions
{
    Core::DeviceManager *deviceManager = nullptr;
    License::LicenseManager *licenseManager = nullptr;
    Project::ProjectManager *projectManager = nullptr;
    QString selfCheckSummary;
};

class DiagnosticPackageExporter
{
public:
    static bool exportPackage(const DiagnosticPackageOptions &options, QString *outputDirectory = nullptr);
};

} // namespace NFSScanner::Diagnostics
