#include "devices/spectrum/SpectrumDeviceHost.h"

#include "devices/spectrum/ISpectrumAnalyzer.h"
#include "devices/spectrum/SpectrumAnalyzerFactory.h"

namespace NFSScanner::Devices::Spectrum {

SpectrumDeviceHost::SpectrumDeviceHost(QObject *parent)
    : QObject(parent)
{
}

SpectrumDeviceHost::~SpectrumDeviceHost()
{
    releaseAnalyzer();
}

ISpectrumAnalyzer *SpectrumDeviceHost::analyzer() const
{
    return analyzer_;
}

bool SpectrumDeviceHost::createAnalyzer(const QString &analyzerName)
{
    releaseAnalyzer();
    analyzer_ = SpectrumAnalyzerFactory::create(analyzerName, nullptr);
    if (!analyzer_) {
        lastError_ = QStringLiteral("无法创建频谱仪：%1").arg(analyzerName);
        return false;
    }

    connect(analyzer_, &ISpectrumAnalyzer::logMessage, this, &SpectrumDeviceHost::logMessage);
    connect(analyzer_, &ISpectrumAnalyzer::connectedChanged, this, &SpectrumDeviceHost::connectedChanged);
    lastError_.clear();
    return true;
}

void SpectrumDeviceHost::releaseAnalyzer()
{
    if (!analyzer_) {
        return;
    }
    if (analyzer_->isConnected()) {
        analyzer_->disconnectDevice();
    }
    analyzer_->deleteLater();
    analyzer_ = nullptr;
}

bool SpectrumDeviceHost::connectDevice(const QVariantMap &options)
{
    if (!analyzer_) {
        lastError_ = QStringLiteral("频谱仪未创建。");
        return false;
    }
    if (!analyzer_->connectDevice(options)) {
        lastError_ = analyzer_->lastError();
        return false;
    }
    lastError_.clear();
    return true;
}

void SpectrumDeviceHost::disconnectDevice()
{
    if (analyzer_) {
        analyzer_->disconnectDevice();
    }
}

bool SpectrumDeviceHost::configureDevice(const SpectrumConfig &config)
{
    if (!analyzer_ || !analyzer_->isConnected()) {
        lastError_ = QStringLiteral("频谱仪未连接。");
        return false;
    }
    if (!analyzer_->configure(config)) {
        lastError_ = analyzer_->lastError();
        return false;
    }
    lastError_.clear();
    return true;
}

QString SpectrumDeviceHost::queryIdn()
{
    if (!analyzer_ || !analyzer_->isConnected()) {
        lastError_ = QStringLiteral("频谱仪未连接。");
        return {};
    }
    const QString idn = analyzer_->queryIdn();
    if (idn.isEmpty()) {
        lastError_ = analyzer_->lastError();
    } else {
        lastError_.clear();
    }
    return idn;
}

SpectrumTrace SpectrumDeviceHost::singleSweep(int pointIndex, double x, double y, double z)
{
    if (!analyzer_ || !analyzer_->isConnected()) {
        lastError_ = QStringLiteral("频谱仪未连接。");
        return {};
    }
    const SpectrumTrace trace = analyzer_->singleSweep(pointIndex, x, y, z);
    if (trace.freqs.isEmpty()) {
        lastError_ = analyzer_->lastError();
    } else {
        lastError_.clear();
    }
    return trace;
}

QString SpectrumDeviceHost::lastError() const
{
    return lastError_;
}

} // namespace NFSScanner::Devices::Spectrum
