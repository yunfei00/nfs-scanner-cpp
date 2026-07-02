#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QIODevice>
#include <QMetaType>
#include <QProcess>

#include "app/AppCommandLine.h"
#include "app/AppVersion.h"
#include "core/ScanPoint.h"
#include "devices/spectrum/SpectrumConfig.h"
#include "ui/MainWindow.h"

#include <cstdio>

namespace {

int runEmbeddedSelfCheck()
{
#ifdef _WIN32
    const QString selfCheckPath = QCoreApplication::applicationDirPath() + QStringLiteral("/NFSScannerSelfCheck.exe");
#else
    const QString selfCheckPath = QCoreApplication::applicationDirPath() + QStringLiteral("/NFSScannerSelfCheck");
#endif
    if (!QFile::exists(selfCheckPath)) {
        std::fprintf(stderr, "SelfCheck executable not found: %s\n", selfCheckPath.toUtf8().constData());
        return 1;
    }
    return QProcess::execute(selfCheckPath, {});
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral(APP_NAME));
    QApplication::setApplicationName(QStringLiteral("NFSScanner"));
    QApplication::setApplicationDisplayName(QStringLiteral(APP_NAME));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

    const NFSScanner::App::AppCommandLineOptions options = NFSScanner::App::AppCommandLine::parse(&app);

    if (options.runSelfCheck) {
        return runEmbeddedSelfCheck();
    }

    qRegisterMetaType<NFSScanner::Core::ScanPoint>("NFSScanner::Core::ScanPoint");
    qRegisterMetaType<NFSScanner::Devices::Spectrum::SpectrumConfig>(
        "NFSScanner::Devices::Spectrum::SpectrumConfig");

    NFSScanner::UI::MainWindow window;
    if (options.safeMode) {
        window.setSafeMode(true);
    }
    if (!options.hardwareConfigPath.isEmpty()) {
        window.loadHardwareConfigFile(options.hardwareConfigPath);
    }
    if (!options.profile.isEmpty()) {
        window.loadHardwareProfile(options.profile);
    }

    if (options.exportDiagnostics) {
        return window.exportDiagnosticsHeadless() ? 0 : 1;
    }

    QFile styleFile(QStringLiteral(":/styles/app.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }

    window.resize(1600, 900);
    window.show();

    return app.exec();
}
