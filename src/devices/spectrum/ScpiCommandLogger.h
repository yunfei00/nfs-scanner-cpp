#pragma once

#include <QString>

namespace NFSScanner::Devices::Spectrum {

class ScpiCommandLogger
{
public:
    struct Entry
    {
        QString deviceType;
        QString host;
        int port = 5025;
        QString command;
        QString responseSummary;
        qint64 elapsedMs = 0;
        bool success = false;
        QString errorMessage;
    };

    static void logEntry(const Entry &entry);
    static QString lastErrorMessage();
    static QString lastIdnResponse();
    static void clearRecentErrors();
};

} // namespace NFSScanner::Devices::Spectrum
