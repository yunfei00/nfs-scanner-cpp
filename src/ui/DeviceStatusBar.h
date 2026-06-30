#pragma once

#include "core/DeviceManager.h"

#include <QWidget>

class QLabel;

namespace NFSScanner::Core {
class DeviceManager;
}

namespace NFSScanner::UI {

class DeviceStatusBar final : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceStatusBar(QWidget *parent = nullptr);

    void bindDeviceManager(NFSScanner::Core::DeviceManager *deviceManager);
    void refresh();

private:
    void applyChipStyle(QLabel *label, NFSScanner::Core::DeviceConnectionState state) const;

    NFSScanner::Core::DeviceManager *deviceManager_ = nullptr;
    QLabel *motionChip_ = nullptr;
    QLabel *spectrumChip_ = nullptr;
    QLabel *cameraChip_ = nullptr;
    QLabel *systemChip_ = nullptr;
};

} // namespace NFSScanner::UI
