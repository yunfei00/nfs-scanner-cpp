#include "devices/spectrum/DeviceBringupResult.h"

#include "devices/spectrum/MockSpectrumAnalyzer.h"
#include "devices/spectrum/SpectrumConfig.h"

namespace NFSScanner::Devices::Spectrum {

QString DeviceBringupResult::markdownReport() const
{
    QString md;
    md += QStringLiteral("# Spectrum Bring-up Report\n\n");
    md += QStringLiteral("- connected: %1\n").arg(connected ? QStringLiteral("yes") : QStringLiteral("no"));
    md += QStringLiteral("- idn: %1\n").arg(idn);
    md += QStringLiteral("- configure: %1\n").arg(configureOk ? QStringLiteral("ok") : QStringLiteral("fail"));
    md += QStringLiteral("- sweep: %1\n").arg(sweepOk ? QStringLiteral("ok") : QStringLiteral("fail"));
    md += QStringLiteral("- trace points: %1\n").arg(tracePointCount);
    md += QStringLiteral("- freq range: %1 ~ %2 Hz\n\n").arg(freqStartHz).arg(freqStopHz);

    md += QStringLiteral("## Steps\n");
    for (const QString &step : steps) {
        md += QStringLiteral("- %1\n").arg(step);
    }
    if (!errors.isEmpty()) {
        md += QStringLiteral("\n## Errors\n");
        for (const QString &error : errors) {
            md += QStringLiteral("- %1\n").arg(error);
        }
    }
    if (!warnings.isEmpty()) {
        md += QStringLiteral("\n## Warnings\n");
        for (const QString &warning : warnings) {
            md += QStringLiteral("- %1\n").arg(warning);
        }
    }
    return md;
}

DeviceBringupResult SpectrumBringupRunner::runMockBringup(const QString &deviceType)
{
    DeviceBringupResult result;
    result.steps.append(QStringLiteral("create mock analyzer (%1)").arg(deviceType));

    MockSpectrumAnalyzer analyzer;
    QVariantMap options;
    options.insert(QStringLiteral("mock"), true);
    result.connected = analyzer.connectDevice(options);
    result.steps.append(result.connected ? QStringLiteral("connect ok") : QStringLiteral("connect fail"));
    if (!result.connected) {
        result.errors.append(analyzer.lastError());
        return result;
    }

    result.idn = analyzer.queryIdn();
    result.idnOk = !result.idn.trimmed().isEmpty();
    result.steps.append(result.idnOk ? QStringLiteral("IDN ok") : QStringLiteral("IDN fail"));

    SpectrumConfig config;
    config.startFreqHz = 1e9;
    config.stopFreqHz = 2e9;
    config.sweepPoints = 101;
    config.traceId = deviceType.contains(QStringLiteral("zna"), Qt::CaseInsensitive) ? QStringLiteral("Trc1_S21")
                                                                                      : QStringLiteral("TRACE1");

    result.configureOk = analyzer.configure(config);
    result.steps.append(result.configureOk ? QStringLiteral("configure ok") : QStringLiteral("configure fail"));

    const SpectrumTrace trace = analyzer.singleSweep(0, 0.0, 0.0, 1.0);
    result.sweepOk = true;
    result.traceOk = !trace.freqs.isEmpty();
    result.tracePointCount = trace.freqs.size();
    if (!trace.freqs.isEmpty()) {
        result.freqStartHz = trace.freqs.first();
        result.freqStopHz = trace.freqs.last();
    }
    result.steps.append(result.traceOk ? QStringLiteral("trace ok") : QStringLiteral("trace fail"));

    analyzer.disconnectDevice();
    return result;
}

} // namespace NFSScanner::Devices::Spectrum
