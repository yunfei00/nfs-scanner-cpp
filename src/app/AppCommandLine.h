#pragma once

#include <QString>

class QCoreApplication;

namespace NFSScanner::App {

struct AppCommandLineOptions
{
    QString profile;
    QString hardwareConfigPath;
    bool runSelfCheck = false;
    bool exportDiagnostics = false;
    bool safeMode = false;
    bool showHelp = false;
    bool parseError = false;
};

class AppCommandLine
{
public:
    static AppCommandLineOptions parse(QCoreApplication *app);
    static QString helpText();
};

} // namespace NFSScanner::App
