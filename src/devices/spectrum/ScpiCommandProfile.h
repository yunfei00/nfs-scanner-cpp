#pragma once

#include <QString>

namespace NFSScanner::Devices::Spectrum {

struct ScpiCommandProfile
{
    QString name;
    QString idnQuery = QStringLiteral("*IDN?");
    QString resetCommand = QStringLiteral("*RST");
    QString clearStatusCommand = QStringLiteral("*CLS");
    QString systemErrorQuery = QStringLiteral("SYST:ERR?");
    QString setStartFreqCommand = QStringLiteral("SENS:FREQ:STAR %1");
    QString setStopFreqCommand = QStringLiteral("SENS:FREQ:STOP %1");
    QString setCenterFreqCommand = QStringLiteral("SENS:FREQ:CENT %1");
    QString setSpanCommand = QStringLiteral("SENS:FREQ:SPAN %1");
    QString setRbwCommand = QStringLiteral("SENS:BAND:RES %1");
    QString setVbwCommand = QStringLiteral("SENS:BAND:VID %1");
    QString setSweepPointsCommand = QStringLiteral("SENS:SWE:POIN %1");
    QString setSweepTimeCommand = QStringLiteral("SENS:SWE:TIME %1");
    QString singleSweepCommand = QStringLiteral("INIT:IMM");
    QString waitOperationCompleteQuery = QStringLiteral("*OPC?");
    QString readTraceCommand = QStringLiteral("TRAC:DATA? TRACE1");
    QString readComplexTraceCommand = QStringLiteral("CALC:DATA? SDATA");

    static ScpiCommandProfile genericDefaults();
    static ScpiCommandProfile zna67Defaults();
    static ScpiCommandProfile fswDefaults();
    static ScpiCommandProfile n9020aDefaults();

    static bool loadFromFile(const QString &path, ScpiCommandProfile *profile, QString *error = nullptr);
    static ScpiCommandProfile loadProfileByType(const QString &deviceType);
};

} // namespace NFSScanner::Devices::Spectrum
