#include "app/CliCommandLine.h"

#include <QCommandLineParser>
#include <QCoreApplication>

namespace NFSScanner::App {

CliCommandLineOptions CliCommandLine::parse(QCoreApplication *app)
{
    CliCommandLineOptions options;

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("NFS Scanner headless validation CLI"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption profileOption({QStringLiteral("profile"), QStringLiteral("p")},
                                     QStringLiteral("Hardware profile name"),
                                     QStringLiteral("name"),
                                     options.profile);
    QCommandLineOption profileDirOption(QStringLiteral("profile-dir"),
                                        QStringLiteral("Directory containing profile JSON files"),
                                        QStringLiteral("path"));
    QCommandLineOption outputOption({QStringLiteral("output"), QStringLiteral("o")},
                                    QStringLiteral("Output directory"),
                                    QStringLiteral("path"),
                                    options.outputDir);
    QCommandLineOption inputOption({QStringLiteral("input"), QStringLiteral("i")},
                                   QStringLiteral("Input directory for report generation"),
                                   QStringLiteral("path"));
    QCommandLineOption portableOption(QStringLiteral("portable-zip"),
                                      QStringLiteral("Portable zip path for validation"),
                                      QStringLiteral("path"));
    QCommandLineOption validateProfilesOption(QStringLiteral("validate-profiles"),
                                              QStringLiteral("Validate hardware profiles"));
    QCommandLineOption runBringupOption(QStringLiteral("run-bringup"), QStringLiteral("Run mock bring-up"));
    QCommandLineOption runMockE2eOption(QStringLiteral("run-mock-e2e"), QStringLiteral("Run mock end-to-end scan"));
    QCommandLineOption exportDiagnosticsOption(QStringLiteral("export-diagnostics"),
                                               QStringLiteral("Export diagnostics package"));
    QCommandLineOption generateReportOption(QStringLiteral("generate-validation-report"),
                                            QStringLiteral("Generate FULL_MOCK_VALIDATION_REPORT.md"));
    QCommandLineOption runFullOption(QStringLiteral("run-full-validation"),
                                     QStringLiteral("Run all mock validation steps"));

    parser.addOption(profileOption);
    parser.addOption(profileDirOption);
    parser.addOption(outputOption);
    parser.addOption(inputOption);
    parser.addOption(portableOption);
    parser.addOption(validateProfilesOption);
    parser.addOption(runBringupOption);
    parser.addOption(runMockE2eOption);
    parser.addOption(exportDiagnosticsOption);
    parser.addOption(generateReportOption);
    parser.addOption(runFullOption);

    parser.process(*app);

    options.profile = parser.value(profileOption);
    options.profileDir = parser.value(profileDirOption);
    options.outputDir = parser.value(outputOption);
    options.inputDir = parser.value(inputOption);
    options.portableZipPath = parser.value(portableOption);
    options.showHelp = parser.isSet(QStringLiteral("help"));
    options.validateProfiles = parser.isSet(validateProfilesOption);
    options.runBringup = parser.isSet(runBringupOption);
    options.runMockE2e = parser.isSet(runMockE2eOption);
    options.exportDiagnostics = parser.isSet(exportDiagnosticsOption);
    options.generateValidationReport = parser.isSet(generateReportOption);
    options.runFullValidation = parser.isSet(runFullOption);

    return options;
}

QString CliCommandLine::helpText()
{
    return QStringLiteral(
        "Usage: NFSScannerCli.exe [options]\n"
        "  --validate-profiles              Validate config/profiles/*.json\n"
        "  --profile <name>                 Profile for bring-up/e2e (default: mock_all)\n"
        "  --profile-dir <path>             Override profiles directory\n"
        "  --run-bringup                    Run hardware bring-up (mock)\n"
        "  --run-mock-e2e                   Run mock scan + analysis validation\n"
        "  --export-diagnostics             Export diagnostics package\n"
        "  --generate-validation-report     Generate validation markdown report\n"
        "  --run-full-validation            Run all in-process mock validations\n"
        "  --output <path>                  Output directory (default: validation_output)\n"
        "  --input <path>                   Input directory for report merge\n"
        "  --portable-zip <path>            Portable zip to verify\n");
}

} // namespace NFSScanner::App
