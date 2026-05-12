#pragma once

#include "devices/spectrum/ISpectrumAnalyzer.h"
#include "devices/spectrum/ScpiTcpClient.h"

namespace NFSScanner::Devices::Spectrum {

class FswSpectrumAnalyzer final : public ISpectrumAnalyzer
{
    Q_OBJECT

public:
    explicit FswSpectrumAnalyzer(QObject *parent = nullptr);

    QString name() const override;
    bool connectDevice(const QVariantMap &options) override;
    void disconnectDevice() override;
    bool isConnected() const override;
    bool configure(const SpectrumConfig &config) override;
    SpectrumTrace singleSweep(int pointIndex, double x, double y, double z) override;
    QString queryIdn() override;
    QString lastError() const override;

private:
    bool waitOpc(int timeoutMs, const QString &context);
    QString normalizeTraceMode(const QString &mode) const;
    bool parseMmemCsv(const QString &rawText, SpectrumTrace *outTrace, QString *error) const;
    void setError(const QString &message);

    ScpiTcpClient client_;
    SpectrumConfig config_;
    QString mmemTempTracePath_ = QStringLiteral(R"(C:\data.csv)");
    double clearWriteSettleSeconds_ = 0.2;
    QString activeTraceName_ = QStringLiteral("TRACE1");
    QString traceMode_ = QStringLiteral("MAXH");
    QString lastError_;
};

} // namespace NFSScanner::Devices::Spectrum
