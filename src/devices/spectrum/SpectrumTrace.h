#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

namespace NFSScanner::Devices::Spectrum {

struct SpectrumTrace
{
    QString traceId = QStringLiteral("Trc1_S21");
    QVector<double> freqs;
    QVector<double> values;
    QDateTime timestamp;
    QString source;
};

} // namespace NFSScanner::Devices::Spectrum
