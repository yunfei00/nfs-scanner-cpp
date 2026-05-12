#include "devices/spectrum/SpectrumAnalyzerFactory.h"

#include "devices/spectrum/GenericScpiSpectrumAnalyzer.h"
#include "devices/spectrum/MockSpectrumAnalyzer.h"
#include "devices/spectrum/Zna67SpectrumAnalyzer.h"

namespace NFSScanner::Devices::Spectrum {

QStringList SpectrumAnalyzerFactory::availableAnalyzers()
{
    return {
        QStringLiteral("Mock Spectrum"),
        QStringLiteral("Generic SCPI"),
        QStringLiteral("R&S ZNA67"),
    };
}

ISpectrumAnalyzer *SpectrumAnalyzerFactory::create(const QString &name, QObject *parent)
{
    const QString normalized = name.trimmed();
    if (normalized == QStringLiteral("Generic SCPI")) {
        return new GenericScpiSpectrumAnalyzer(parent);
    }
    if (normalized == QStringLiteral("R&S ZNA67")) {
        return new Zna67SpectrumAnalyzer(parent);
    }
    return new MockSpectrumAnalyzer(parent);
}

} // namespace NFSScanner::Devices::Spectrum
