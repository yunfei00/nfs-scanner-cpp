#pragma once

#include "core/ScanConfig.h"
#include "core/ScanPoint.h"
#include "devices/spectrum/ISpectrumAnalyzer.h"
#include "devices/spectrum/MockSpectrumAnalyzer.h"
#include "devices/spectrum/SpectrumConfig.h"
#include "storage/TaskStorage.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

namespace NFSScanner::Core {

enum class ScanState {
    Idle,
    Preparing,
    Running,
    Paused,
    Stopping,
    Stopped,
    Finished,
    Error
};

QString scanStateToString(ScanState state);

class ScanManager final : public QObject
{
    Q_OBJECT

public:
    explicit ScanManager(QObject *parent = nullptr);
    ~ScanManager() override;

    void startScan(const ScanConfig &config);
    void pauseScan();
    void resumeScan();
    void stopScan();
    void setSpectrumAnalyzer(NFSScanner::Devices::Spectrum::ISpectrumAnalyzer *analyzer);
    void setSpectrumConfig(const NFSScanner::Devices::Spectrum::SpectrumConfig &config);

    ScanState state() const;
    int currentIndex() const;
    int totalCount() const;

signals:
    void stateChanged(const QString &stateText);
    void logMessage(const QString &message);
    void progressChanged(int current, int total);
    void currentPointChanged(int index, int total, double x, double y, double z);
    void estimatedChanged(int remainingCount, int estimatedSeconds);
    void taskDirChanged(const QString &taskDir);
    void scanFinished(const QString &taskDir);
    void scanError(const QString &message);

private:
    void advanceScan();
    void setState(ScanState state);

    ScanConfig config_;
    QVector<ScanPoint> points_;
    NFSScanner::Storage::TaskStorage storage_;
    NFSScanner::Devices::Spectrum::ISpectrumAnalyzer *analyzer_ = nullptr;
    NFSScanner::Devices::Spectrum::SpectrumConfig spectrumConfig_;
    NFSScanner::Devices::Spectrum::MockSpectrumAnalyzer fallbackSpectrum_;
    QTimer timer_;
    ScanState state_ = ScanState::Idle;
    int currentIndex_ = 0;
};

} // namespace NFSScanner::Core
