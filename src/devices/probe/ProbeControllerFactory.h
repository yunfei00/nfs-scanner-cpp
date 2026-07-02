#pragma once

#include "devices/probe/IProbeController.h"

namespace NFSScanner::Devices::Probe {

class SerialProbeControllerStub final : public IProbeController
{
public:
    explicit SerialProbeControllerStub(QObject *parent = nullptr);

    QString name() const override;
    bool connectDevice() override;
    void disconnectDevice() override;
    bool isConnected() const override;
    bool setOrientation(ProbeOrientation orientation) override;
    ProbeOrientation currentOrientation() const override;
    QString lastError() const override;

private:
    ProbeOrientation orientation_ = ProbeOrientation::Hx;
    QString lastError_;
};

class SdkProbeControllerStub final : public IProbeController
{
public:
    explicit SdkProbeControllerStub(QObject *parent = nullptr);

    QString name() const override;
    bool connectDevice() override;
    void disconnectDevice() override;
    bool isConnected() const override;
    bool setOrientation(ProbeOrientation orientation) override;
    ProbeOrientation currentOrientation() const override;
    QString lastError() const override;

private:
    ProbeOrientation orientation_ = ProbeOrientation::Hx;
    QString lastError_;
};

IProbeController *createProbeController(const QString &type, QObject *parent = nullptr);

} // namespace NFSScanner::Devices::Probe
