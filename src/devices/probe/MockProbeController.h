#pragma once

#include "devices/probe/IProbeController.h"

namespace NFSScanner::Devices::Probe {

class MockProbeController final : public IProbeController
{
    Q_OBJECT

public:
    explicit MockProbeController(QObject *parent = nullptr);

    QString name() const override;
    bool connectDevice() override;
    void disconnectDevice() override;
    bool isConnected() const override;
    bool setOrientation(ProbeOrientation orientation) override;
    ProbeOrientation currentOrientation() const override;
    QString lastError() const override;

private:
    bool connected_ = false;
    ProbeOrientation orientation_ = ProbeOrientation::Hx;
    QString lastError_;
    int switchDelayMs_ = 500;
};

} // namespace NFSScanner::Devices::Probe
