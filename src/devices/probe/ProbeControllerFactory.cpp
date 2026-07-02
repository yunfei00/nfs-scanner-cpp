#include "devices/probe/ProbeControllerFactory.h"

#include "devices/probe/MockProbeController.h"

namespace NFSScanner::Devices::Probe {

namespace {

QString stubError()
{
    return QStringLiteral("真实探头控制尚未接入，请使用 Mock 模式或等待现场接线确认。");
}

} // namespace

SerialProbeControllerStub::SerialProbeControllerStub(QObject *parent)
    : IProbeController(parent)
{
}

QString SerialProbeControllerStub::name() const
{
    return QStringLiteral("Serial Probe (Stub)");
}

bool SerialProbeControllerStub::connectDevice()
{
    lastError_ = stubError();
    return false;
}

void SerialProbeControllerStub::disconnectDevice()
{
}

bool SerialProbeControllerStub::isConnected() const
{
    return false;
}

bool SerialProbeControllerStub::setOrientation(ProbeOrientation orientation)
{
    Q_UNUSED(orientation)
    lastError_ = stubError();
    return false;
}

ProbeOrientation SerialProbeControllerStub::currentOrientation() const
{
    return orientation_;
}

QString SerialProbeControllerStub::lastError() const
{
    return lastError_;
}

SdkProbeControllerStub::SdkProbeControllerStub(QObject *parent)
    : IProbeController(parent)
{
}

QString SdkProbeControllerStub::name() const
{
    return QStringLiteral("SDK Probe (Stub)");
}

bool SdkProbeControllerStub::connectDevice()
{
    lastError_ = stubError();
    return false;
}

void SdkProbeControllerStub::disconnectDevice()
{
}

bool SdkProbeControllerStub::isConnected() const
{
    return false;
}

bool SdkProbeControllerStub::setOrientation(ProbeOrientation orientation)
{
    Q_UNUSED(orientation)
    lastError_ = stubError();
    return false;
}

ProbeOrientation SdkProbeControllerStub::currentOrientation() const
{
    return orientation_;
}

QString SdkProbeControllerStub::lastError() const
{
    return lastError_;
}

IProbeController *createProbeController(const QString &type, QObject *parent)
{
    const QString normalized = type.trimmed().toLower();
    if (normalized.isEmpty() || normalized == QStringLiteral("mock")) {
        return new MockProbeController(parent);
    }
    if (normalized == QStringLiteral("serial")) {
        return new SerialProbeControllerStub(parent);
    }
    if (normalized == QStringLiteral("sdk")) {
        return new SdkProbeControllerStub(parent);
    }
    return new MockProbeController(parent);
}

} // namespace NFSScanner::Devices::Probe
