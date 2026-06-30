#pragma once

#include "analysis/FrequencyData.h"

#include <QImage>
#include <QObject>
#include <QString>

class QTimer;

namespace NFSScanner::UI {

struct AnalysisRenderParams
{
    QString traceId;
    int freqIndex = 0;
    QString mode = QStringLiteral("magnitude");
    QString lutName = QStringLiteral("turbo");
    bool autoRange = true;
    double vmin = 0.0;
    double vmax = 1.0;
    int opacityPercent = 85;
    int width = 900;
    int height = 600;
};

class AnalysisController final : public QObject
{
    Q_OBJECT

public:
    explicit AnalysisController(QObject *parent = nullptr);

    const Analysis::FrequencyData &frequencyData() const;
    QImage heatmapImage() const;
    QImage colorbarImage() const;
    double vmin() const;
    double vmax() const;

    bool loadTraceCsv(const QString &path, QString *errorMessage = nullptr);
    bool refreshPreview(const AnalysisRenderParams &params, QString *errorMessage = nullptr);
    void scheduleRefresh(const AnalysisRenderParams &params, int debounceMs = 300);

signals:
    void previewUpdated();
    void loadFailed(const QString &message);
    void actualRangeUpdated(double vmin, double vmax);

private:
    void onDebounceTimeout();

    Analysis::FrequencyData frequencyData_;
    QImage heatmapImage_;
    QImage colorbarImage_;
    double vmin_ = 0.0;
    double vmax_ = 1.0;
    QTimer *debounceTimer_ = nullptr;
    AnalysisRenderParams pendingParams_;
    bool hasPendingParams_ = false;
};

} // namespace NFSScanner::UI
