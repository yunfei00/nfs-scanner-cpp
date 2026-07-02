#include "app/AppCommandLine.h"

#include <QCommandLineParser>
#include <QCoreApplication>

namespace NFSScanner::App {

AppCommandLineOptions AppCommandLine::parse(QCoreApplication *app)
{
    AppCommandLineOptions options;

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("NFS Scanner near-field scanning system"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption profileOption({QStringLiteral("profile"), QStringLiteral("p")},
                                     QStringLiteral("Load hardware profile from config/profiles/"),
                                     QStringLiteral("name"));
    QCommandLineOption configOption({QStringLiteral("hardware-config"), QStringLiteral("c")},
                                    QStringLiteral("Load hardware config JSON file"),
                                    QStringLiteral("path"));
    QCommandLineOption selfCheckOption(QStringLiteral("self-check"), QStringLiteral("Run self-check and exit"));
    QCommandLineOption diagnosticsOption(QStringLiteral("export-diagnostics"), QStringLiteral("Export diagnostics package and exit"));
    QCommandLineOption safeModeOption(QStringLiteral("safe-mode"), QStringLiteral("Disable automatic real hardware connection"));

    parser.addOption(profileOption);
    parser.addOption(configOption);
    parser.addOption(selfCheckOption);
    parser.addOption(diagnosticsOption);
    parser.addOption(safeModeOption);

    parser.process(*app);

    if (parser.isSet(profileOption)) {
        options.profile = parser.value(profileOption);
    }
    if (parser.isSet(configOption)) {
        options.hardwareConfigPath = parser.value(configOption);
    }
    options.runSelfCheck = parser.isSet(selfCheckOption);
    options.exportDiagnostics = parser.isSet(diagnosticsOption);
    options.safeMode = parser.isSet(safeModeOption);
    options.showHelp = parser.isSet(QStringLiteral("help"));

    return options;
}

QString AppCommandLine::helpText()
{
    return QStringLiteral(
        "Usage: NFSScanner.exe [options]\n"
        "  --profile <name>           Load profile from config/profiles/\n"
        "  --hardware-config <path>   Load hardware_config.json\n"
        "  --self-check               Run NFSScannerSelfCheck logic and exit\n"
        "  --export-diagnostics       Export diagnostics package and exit\n"
        "  --safe-mode                Do not auto-connect real hardware\n");
}

} // namespace NFSScanner::App
