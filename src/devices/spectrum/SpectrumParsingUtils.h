#pragma once

#include <QString>
#include <QVector>

namespace NFSScanner::Devices::Spectrum::Parsing {

QString stripScpiBlockHeader(const QString &rawText, QString *error = nullptr);
QVector<double> parseAsciiFloatValues(const QString &rawText, QString *error = nullptr);
QVector<double> buildFrequencyAxis(double startHz, double stopHz, int pointCount);
double magnitudeToDb(double re, double im);
QString formatCoordKey(double x, double y, double z);
double parseFrequencyValue(const QString &text, bool *ok = nullptr);
void normalizeFrequencyWindow(double *startHz, double *stopHz, double *centerHz, double *spanHz);

} // namespace NFSScanner::Devices::Spectrum::Parsing
