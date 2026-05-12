#pragma once

#include <QString>
#include <QStringList>

class QObject;

namespace NFSScanner::Devices::Spectrum {

class ISpectrumAnalyzer;

class SpectrumAnalyzerFactory
{
public:
    static QStringList availableAnalyzers();
    static ISpectrumAnalyzer *create(const QString &name, QObject *parent = nullptr);
};

} // namespace NFSScanner::Devices::Spectrum
