#include <QApplication>
#include <QFile>
#include <QIODevice>
#include <QMetaType>

#include "core/ScanPoint.h"
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("NFS Scanner"));
    QApplication::setApplicationName(QStringLiteral("NFSScanner"));
    QApplication::setApplicationDisplayName(QStringLiteral("NFS Scanner"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.1"));

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
