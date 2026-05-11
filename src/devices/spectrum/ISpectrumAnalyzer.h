#pragma once

#include <QObject>
#include <QString>

namespace NFSScanner::Devices::Spectrum {

class ISpectrumAnalyzer : public QObject
{
    Q_OBJECT

public:
    explicit ISpectrumAnalyzer(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~ISpectrumAnalyzer() override = default;

    virtual QString name() const = 0;
    virtual bool connectDevice() = 0;
    virtual void disconnectDevice() = 0;
    virtual bool isConnected() const = 0;
    virtual void setCenterFrequency(double mhz) = 0;
    virtual double centerFrequencyMhz() const = 0;
    virtual double readPowerDbm() = 0;

signals:
    void connectionChanged(bool connected);
    void centerFrequencyChanged(double mhz);
    void errorOccurred(const QString &message);
};

} // namespace NFSScanner::Devices::Spectrum
