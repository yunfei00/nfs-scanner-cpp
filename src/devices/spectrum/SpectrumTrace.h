#pragma once

#include <QDateTime>
#include <QString>
#include <QVariantMap>
#include <QVector>

namespace NFSScanner::Devices::Spectrum {

struct SpectrumTrace
{
    struct TraceComponent
    {
        QString traceId;
        QVector<double> realValues;
        QVector<double> imagValues;
        QVector<double> displayValues;
    };

    QString traceId = QStringLiteral("Trc1_S21");
    QVector<double> freqs;

    // Main display values. These are amplitude, magnitude, or dB depending on source.
    QVector<double> values;

    // Optional complex data for instruments that expose re/im trace rows.
    QVector<double> realValues;
    QVector<double> imagValues;

    // Multiple traces returned by one acquisition, for example ZNA67 MMEM CSV.
    QVector<TraceComponent> components;

    QDateTime timestamp;
    QString source;
    QVariantMap metadata;
};

} // namespace NFSScanner::Devices::Spectrum
