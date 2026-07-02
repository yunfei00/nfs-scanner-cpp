#include "devices/camera/MockCamera.h"

#include "devices/FaultInjectionConfig.h"
#include "diagnostics/HardwareSessionRecorder.h"

#include <QPainter>
#include <QRandomGenerator>

namespace NFSScanner::Devices::Camera {

MockCamera::MockCamera(QObject *parent)
    : ICamera(parent)
{
}

QString MockCamera::name() const
{
    return QStringLiteral("Mock Camera");
}

bool MockCamera::connectDevice(const QVariantMap &options)
{
    Q_UNUSED(options)
    connected_ = true;
    lastPreview_ = generateMockFrame();
    emit logMessage(QStringLiteral("Mock Camera 已连接。"));
    emit connectedChanged(true);
    return true;
}

void MockCamera::disconnectDevice()
{
    connected_ = false;
    emit logMessage(QStringLiteral("Mock Camera 已断开。"));
    emit connectedChanged(false);
}

bool MockCamera::isConnected() const
{
    return connected_;
}

QImage MockCamera::captureFrame()
{
    if (!connected_) {
        lastError_ = QStringLiteral("相机未连接。");
        return {};
    }

    FaultInjectionConfig &fault = globalFaultInjectionConfig();
    if (fault.enabled && fault.cameraCaptureFail) {
        lastError_ = QStringLiteral("Fault injection: camera_capture_fail");
        Diagnostics::HardwareSessionRecorder::recordEvent(QStringLiteral("camera"), QStringLiteral("Camera"), lastError_, false);
        return {};
    }

    lastPreview_ = generateMockFrame();
    emit frameCaptured(lastPreview_);
    emit logMessage(QStringLiteral("Mock Camera 已拍照。"));
    return lastPreview_;
}

QImage MockCamera::lastPreview() const
{
    return lastPreview_;
}

QString MockCamera::lastError() const
{
    return lastError_;
}

QImage MockCamera::generateMockFrame() const
{
    constexpr int width = 640;
    constexpr int height = 480;
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(QColor(20, 48, 32));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(180, 150, 60), 2));
    for (int i = 0; i < 12; ++i) {
        const int x = 40 + i * 48;
        painter.drawLine(x, 60, x + 20, height - 60);
    }
    painter.setPen(QColor(220, 230, 240));
    painter.setFont(QFont(QStringLiteral("Segoe UI"), 11));
    painter.drawText(20, 30, QStringLiteral("Mock Camera Preview"));
    painter.drawText(20, height - 20,
                     QStringLiteral("frame #%1")
                         .arg(QRandomGenerator::global()->bounded(10000)));
    return image;
}

} // namespace NFSScanner::Devices::Camera
