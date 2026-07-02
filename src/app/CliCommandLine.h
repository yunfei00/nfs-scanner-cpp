#pragma once

#include <QString>

class QCoreApplication;

namespace NFSScanner::App {

struct CliCommandLineOptions
{
    QString profile = QStringLiteral("mock_all");
    QString profileDir;
    QString outputDir = QStringLiteral("validation_output");
    QString inputDir;
    QString portableZipPath;

    bool showHelp = false;
    bool validateProfiles = false;
    bool runBringup = false;
    bool runMockE2e = false;
    bool exportDiagnostics = false;
    bool generateValidationReport = false;
    bool runFullValidation = false;
};

class CliCommandLine
{
public:
    static CliCommandLineOptions parse(QCoreApplication *app);
    static QString helpText();
};

} // namespace NFSScanner::App
