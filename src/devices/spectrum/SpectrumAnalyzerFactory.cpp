#include "devices/spectrum/SpectrumAnalyzerFactory.h"

#include "devices/spectrum/FswSpectrumAnalyzer.h"
#include "devices/spectrum/GenericScpiSpectrumAnalyzer.h"
#include "devices/spectrum/MockSpectrumAnalyzer.h"
#include "devices/spectrum/N9020aSpectrumAnalyzer.h"
#include "devices/spectrum/Zna67SpectrumAnalyzer.h"

namespace NFSScanner::Devices::Spectrum {

QStringList SpectrumAnalyzerFactory::availableAnalyzers()
{
    return {
        QStringLiteral("Mock Spectrum"),
        QStringLiteral("Generic SCPI"),
        QStringLiteral("R&S ZNA67"),
        QStringLiteral("R&S FSW"),
        QStringLiteral("Keysight N9020A"),
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
    if (normalized == QStringLiteral("R&S FSW")) {
        return new FswSpectrumAnalyzer(parent);
    }
    if (normalized == QStringLiteral("Keysight N9020A")) {
        return new N9020aSpectrumAnalyzer(parent);
    }
    return new MockSpectrumAnalyzer(parent);
}

} // namespace NFSScanner::Devices::Spectrum
