#pragma once

#include "devices/spectrum/ISpectrumAnalyzer.h"
#include "devices/spectrum/ScpiTcpClient.h"

namespace NFSScanner::Devices::Spectrum {

class N9020aSpectrumAnalyzer final : public ISpectrumAnalyzer
{
    Q_OBJECT

public:
    explicit N9020aSpectrumAnalyzer(QObject *parent = nullptr);

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
    double queryDoubleOrFallback(const QString &command, double fallback) const;
    int queryIntOrFallback(const QString &command, int fallback) const;
    QString normalizeTraceMode(const QString &mode) const;
    SpectrumTrace traceFromNumbers(const QVector<double> &numbers) const;
    void setError(const QString &message);

    ScpiTcpClient client_;
    SpectrumConfig config_;
    QString cachedIdnText_;
    QString traceMode_ = QStringLiteral("WRIT");
    QString lastError_;
};

} // namespace NFSScanner::Devices::Spectrum
