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
    QVector<double> parseAsciiNumbers(const QByteArray &payload) const;
    QVector<double> buildFrequencies(int pointCount) const;
    void setError(const QString &message);

    ScpiTcpClient client_;
    SpectrumConfig config_;
    QString lastError_;
};

} // namespace NFSScanner::Devices::Spectrum
