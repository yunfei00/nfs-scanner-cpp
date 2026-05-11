#include <QApplication>
#include <QFile>
#include <QIODevice>
#include <QMetaType>

#include "app/AppVersion.h"
#include "core/ScanPoint.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral(APP_NAME));
    QApplication::setApplicationName(QStringLiteral("NFSScanner"));
    QApplication::setApplicationDisplayName(QStringLiteral(APP_NAME));
    QApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

    qRegisterMetaType<NFSScanner::Core::ScanPoint>("NFSScanner::Core::ScanPoint");

    QFile styleFile(QStringLiteral(":/styles/app.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }

    NFSScanner::UI::MainWindow window;
    window.resize(1600, 900);
    window.show();

    return app.exec();
}
