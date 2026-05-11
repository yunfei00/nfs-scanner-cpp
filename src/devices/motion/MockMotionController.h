#pragma once

#include "devices/motion/IMotionController.h"

namespace NFSScanner::Devices::Motion {

class MockMotionController final : public IMotionController
{
    Q_OBJECT

public:
    explicit MockMotionController(QObject *parent = nullptr);

    QString name() const override;
    bool connectDevice() override;
    void disconnectDevice() override;
    bool isConnected() const override;
    bool moveTo(double xMm, double yMm, double zMm) override;

private:
    bool connected_ = false;
    double xMm_ = 0.0;
    double yMm_ = 0.0;
    double zMm_ = 0.0;
};

} // namespace NFSScanner::Devices::Motion
