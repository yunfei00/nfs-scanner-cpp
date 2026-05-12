#pragma once

#include "devices/spectrum/ISpectrumAnalyzer.h"
#include "devices/spectrum/SpectrumAcquisitionRequest.h"
#include "devices/spectrum/SpectrumAcquisitionResult.h"

#include <QObject>

namespace NFSScanner::Devices::Spectrum {

class SpectrumAcquisitionWorker final : public QObject
{
    Q_OBJECT

public:
    explicit SpectrumAcquisitionWorker(QObject *parent = nullptr);

    void setAnalyzer(ISpectrumAnalyzer *analyzer);
    void setFallbackAnalyzer(ISpectrumAnalyzer *fallbackAnalyzer);
    void setStopOnError(bool stopOnError);
    void setRetryCount(int retryCount);

public slots:
    void acquire(const SpectrumAcquisitionRequest &request);

signals:
    void acquisitionStarted(int pointIndex);
    void acquisitionFinished(const SpectrumAcquisitionResult &result);
    void logMessage(const QString &message);
    void errorOccurred(const QString &message);

private:
    ISpectrumAnalyzer *activeAnalyzer() const;

    ISpectrumAnalyzer *analyzer_ = nullptr;
    ISpectrumAnalyzer *fallbackAnalyzer_ = nullptr;
    bool stopOnError_ = true;
    int retryCount_ = 0;
};

} // namespace NFSScanner::Devices::Spectrum
