#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include "core/ScanConfig.h"
#include "core/ScanPoint.h"

namespace NFSScanner::Core {

class ScanWorker final : public QObject
{
    Q_OBJECT

public:
    explicit ScanWorker(QObject *parent = nullptr);

    void setConfig(const ScanConfig &config);
    const ScanConfig &config() const;
    bool isRunning() const;

public slots:
    void startMockScan();
    void stopScan();

signals:
    void progressChanged(int currentPoint, int totalPoints, const NFSScanner::Core::ScanPoint &point);
    void logMessage(const QString &message);
    void scanFinished(bool completed);

private slots:
    void advanceMockPoint();

private:
    ScanPoint createPoint(int index) const;
    void finishScan(bool completed);

    ScanConfig config_;
    QTimer timer_;
    int currentIndex_ = 0;
    bool running_ = false;
};

} // namespace NFSScanner::Core
