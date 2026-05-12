#pragma once

#include "devices/spectrum/ISpectrumAnalyzer.h"
#include "devices/spectrum/ScpiTcpClient.h"

namespace NFSScanner::Devices::Spectrum {

class Zna67SpectrumAnalyzer final : public ISpectrumAnalyzer
{
    Q_OBJECT

public:
    explicit Zna67SpectrumAnalyzer(QObject *parent = nullptr);

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
    bool parseZnaMmemCsv(const QString &rawText,
                         double x,
                         double y,
                         double z,
                         SpectrumTrace *outTrace,
                         QString *error) const;
    SpectrumTrace fallbackSDataSweep();
    void setError(const QString &message);

    ScpiTcpClient client_;
    SpectrumConfig config_;
    QString mmemTempTracePath_ = QStringLiteral(R"(C:\temp\data.csv)");
    QString lastError_;
};

} // namespace NFSScanner::Devices::Spectrum
