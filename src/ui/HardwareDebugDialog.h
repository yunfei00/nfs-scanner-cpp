#pragma once

#include <QDialog>

class QPlainTextEdit;
class QTabWidget;

namespace NFSScanner::Core {
class DeviceManager;
}

namespace NFSScanner::Devices::Motion {
class SerialMotionController;
}

namespace NFSScanner::UI {

class HardwareDebugDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit HardwareDebugDialog(NFSScanner::Core::DeviceManager *deviceManager,
                                 NFSScanner::Devices::Motion::SerialMotionController *motionController,
                                 QWidget *parent = nullptr);

private:
    void appendLog(const QString &text);
    QWidget *buildMotionTab();
    QWidget *buildSpectrumTab();
    QWidget *buildCameraTab();
    QWidget *buildProbeTab();
    QWidget *buildFaultInjectionTab();

    NFSScanner::Core::DeviceManager *deviceManager_ = nullptr;
    NFSScanner::Devices::Motion::SerialMotionController *motionController_ = nullptr;
    QPlainTextEdit *logEdit_ = nullptr;
};

} // namespace NFSScanner::UI
