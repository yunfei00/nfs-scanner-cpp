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

struct HardwareDiagnosticsOptions
{
    QString projectPath;
    NFSScanner::License::LicenseManager *licenseManager = nullptr;
    NFSScanner::Project::ProjectManager *projectManager = nullptr;
    NFSScanner::Core::DeviceManager *deviceManager = nullptr;
    int recentLogLineCount = 100;
};

class HardwareDiagnostics
{
public:
    static QString buildMarkdownSummary(const HardwareDiagnosticsOptions &options);
    static bool exportMarkdownReport(const HardwareDiagnosticsOptions &options, QString *outputPath = nullptr);
};

} // namespace NFSScanner::Diagnostics
